#!/usr/bin/env python
# -*- coding: utf-8 -*-
import sys
import os
import re
import logging
import argparse
import platform
import subprocess
import threading
import time
import shutil
import stat
import uuid

LOG_FILE = os.path.join(os.curdir, 'build-wrapper.log')
LOGGING_FMT = '%(asctime)s - %(levelname)3s] %(filename)s::%(funcName)s(%(lineno)d) - %(message)s'

REG_MODULE_HASH = re.compile('^MODULE \w+ \w+ (\w+) .*$')

if sys.platform == 'win32':
  print "FATAL. Windows is not supported yet."
  sys.exit(0)

RUN_FUZZER_SCRIPT = """#!/usr/bin/env bash
FUZZER_PATH=%s
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$FUZZER_PATH
export DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:$FUZZER_PATH
export DYLD_FALLBACK_LIBRARY_PATH=$DYLD_FALLBACK_LIBRARY_PATH:$FUZZER_PATH

OS=`uname -s`
if [ "$OS" == "Darwin" ]; then
  # Disable crash reporter for OSX
  echo "skipped"
  # launchctl unload -w /System/Library/LaunchAgents/com.apple.ReportCrash.plist
  # sudo launchctl unload -w /System/Library/LaunchDaemons/com.apple.ReportCrash.Root.plist
fi

rm fuzzing.log
$FUZZER_PATH/coverage-fuzz --models=../models.xxx --stream-target-stdout=false --target-symbols=%s --ui-docroot=$FUZZER_PATH/ui -- ./%s __INPUT__
"""

SYSTEM_DYNAMIC_EXTENSION = 'dylib' if sys.platform == 'darwin' else 'so'
SYSTEM_EXTENSION = '.exe' if sys.platform == 'win' else ''


def make_uuid():
  return str(uuid.uuid4()).replace('-', '').upper()


def removeOtherHandlers(to_keep=None):
  for hdl in logger.handlers:
    if hdl != to_keep:
      logger.removeHandler(hdl)


def enableLogger(to_file=None, logging_level=logging.ERROR):
  logger.setLevel(logging_level)
  ch = logging.StreamHandler() if not to_file else logging.FileHandler(to_file, mode='w')
  ch.setLevel(logging_level)
  fmt = logging.Formatter(LOGGING_FMT)
  ch.setFormatter(fmt)
  logger.addHandler(ch)
  removeOtherHandlers(ch)

logger = logging.getLogger('wrapper')
removeOtherHandlers()

COVERAGE_INSTR_FINAL_BINARY = "COVERAGE_INSTR_FINAL_BINARY="

MODELS_XXX = "models.xxx"
DEFAULT_INSTR_DIR = 'instr_idir'
INSTRUMENT_TARGET = 'libclang-instrument.' + SYSTEM_DYNAMIC_EXTENSION
PINTOOL_TARGET = 'intercept-spawn.' + SYSTEM_DYNAMIC_EXTENSION
LIB_CXX_ABI = 'libc++abi.1.' + SYSTEM_DYNAMIC_EXTENSION
RUNTIME_A = 'libinstr-runtime.a'
SYMBOLIZER = 'dump_syms' + SYSTEM_EXTENSION

normalize_path = lambda x: os.path.abspath(x)

def file_extension(filename):
  if '.' not in filename:
    return None
  return filename[filename.rfind('.') + 1:]

def good_ext(fext, l=None):
  return fext.lower() in l if l else False

def scan_dir(directory, files, l_ext=None):
  names = os.listdir(directory)
  for name in names:
    srcname = normalize_path(os.path.join(directory, name))
    try:
      if os.path.isdir(srcname):
        try:
          scan_dir(srcname, files, l_ext)
        except:
          continue
      elif os.path.isfile(srcname) \
           and (not l_ext \
                or good_ext(srcname[srcname.rfind('.')+1:], l_ext)):
        if srcname not in files:
          files.append(srcname)
    except IOError, error:
      continue

def list_dir(directory):
  subdirs = os.listdir(directory)
  if not subdirs:
    return []
  return [os.path.join(directory, k) for k in subdirs]

def build_dir(rts):
  ret = ''
  abs_path = False
  if rts.startswith(os.sep):
    abs_path = True

  dirs = rts.split(os.sep)
  for d in dirs:
    try:
      ret += d + os.sep
      if abs_path:
        ret = os.sep + ret
        abs_path = False
      os.mkdir(ret)
    except:
      continue
  return ret

def clean_dir(directory):
  try:
    shutil.rmtree(directory, ignore_errors=True)
    return build_dir(directory)
  except:
    return directory

def get_options():
  argc, argv = len(sys.argv), sys.argv
  parser = argparse.ArgumentParser(description='CovFuzzer Build Wrapper')
  parser.add_argument('--debug', type=bool, default=True, action='store',
                     help='an integer for the accumulator')
  parser.add_argument('--dist', type=str, required=True, default=os.curdir,
                     help='Path to the install of coverage-fuzzer')
  parser.add_argument('--instr_dir', type=str, required=False, default=os.path.join(os.curdir, DEFAULT_INSTR_DIR),
                     help='Name of the directory that will receive the instrumented program')
  parser.add_argument('--llvm_dir', type=str, required=False, default=None,
                     help='LLVM install directory or embedded one')
  args, unknown = parser.parse_known_args()
  return args, ' '.join(unknown)


class ProcessLauncher(object):
  def __init__(self, build_wrapper):
    self._build_wrapper = build_wrapper
    self._final_binary = None
    self._symbs_file = None
    self._simple_syms_dir = None

  @property
  def build_wrapper(self):
    return self._build_wrapper

  @property
  def final_binary(self):
    return self._final_binary


  @staticmethod
  def execute(argv, cwd=None, env=os.environ, callback_out=None,
              callback_err=None, verbose=False):
    if verbose:
      logger.debug("execute: %s", ' '.join(argv) if not isinstance(argv, basestring) else argv)
    try:
      if not isinstance(argv, basestring):
        argv = ' '.join(argv)
      p = subprocess.Popen(argv,
                           shell=True, cwd=cwd,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           env=env, bufsize=65535)
      output, errors = p.communicate()
      if verbose and not callback_out and not callback_err:
        logger.debug("output: %s", output)
        logger.debug("errors: %s", errors)

      if callback_out is not None: callback_out(output)
      if callback_err is not None: callback_err(errors)

      return output
    except subprocess.CalledProcessError, ex:
      logger.error("Exception while executing: %s", argv)
      logger.error("CalledProcessError- %s", ex, exc_info=True)
      raise subprocess.CalledProcessError(repr(ex))

  def run(self):
    cmd_line = self.make_wrapped_command()

    # That's how we get the name of the generated lib
    def store_executable_name(output):
      logger.debug("Output: \n%s", output)
      lines = output.split('\n')
      for x in lines:
        if x.startswith(COVERAGE_INSTR_FINAL_BINARY):
          self._final_binary = x[len(COVERAGE_INSTR_FINAL_BINARY):]

    def to_log_file(output):
      logger.debug("Error: \n%s", output)


    ProcessLauncher.execute(cmd_line, cwd=self.build_wrapper.start_dir, verbose=True, callback_out=store_executable_name, callback_err=to_log_file)
    self.symbolize()

  def symbolize(self):
    """
      When the instrumented binary is generated, we call the symbolizer on it. This
      is later used in the fuzzer to get actual nice traces from the minidumps.
    """
    if not self.final_binary:
      return
    dump_syms = self.build_wrapper.path_to(SYMBOLIZER)
    syms_in = os.path.join(self.build_wrapper.instr_dir, self.final_binary)
    cmd_line = '%s -c "%s"' % (dump_syms, syms_in)

    self._internal_buffer = ''
    def store_contents(output):
      self._internal_buffer = output
    ProcessLauncher.execute(cmd_line, cwd=self.build_wrapper.start_dir, verbose=True, callback_out=store_contents)

    module_hash = self._internal_buffer.split('\n')[0]
    results = REG_MODULE_HASH.findall(module_hash)
    if not results:
      logger.error("Cannot find module hash in: %s", module_hash)
      module_hash = make_uuid()
    else:
      module_hash = results[0]

    self._simple_syms_dir = 'bin.symbols'
    self._syms_dir = os.path.join(self.build_wrapper.instr_dir, self._simple_syms_dir)

    simple_syms_dir = self.final_binary
    self._symbs_file = self.final_binary + '.sym'
    syms_dir = os.path.join(self._syms_dir, simple_syms_dir)
    syms_dir = os.path.join(syms_dir, module_hash)
    syms_dir = build_dir(syms_dir)
    syms_output = os.path.join(syms_dir, self._symbs_file)

    fd = open(syms_output, 'w')
    fd.write(self._internal_buffer)
    fd.close()




  def make_wrapped_command(self):
    """
      Returns the command that needs to be executed. That calls Pin with our tool
      onto the build system
    """
    return '%s -mt -follow_execv -t "%s" -d "%s" -c "%s" -x "%s" -i "%s" -t "%s" -- %s' \
           % (self.build_wrapper.path_to('pin'),
              self.build_wrapper.path_to(PINTOOL_TARGET),
              self.build_wrapper.instr_dir,
              os.path.dirname(self.build_wrapper.path_to('clang')),
              os.path.dirname(self.build_wrapper.path_to(LIB_CXX_ABI)),
              self.build_wrapper.path_to(INSTRUMENT_TARGET),
              self.build_wrapper.path_to(RUNTIME_A),
              self.build_wrapper.build_command)


  def detach(self):
    if self._session:
      self._session.detach()

  def start(self):
    try:
      self.run()
    except Exception, ex:
      logger.error("Exception- %s", str(ex), exc_info=True)
      return False
    return True


class BuildWrapper(object):
  def __init__(self, args, build_command):
    self._args = args
    self._build_command = build_command
    self._start_dir = os.path.abspath(os.curdir)
    self._process_launcher = None
    self._instr_dir = None
    self._files = {}
    self.__inspect_install_dir()

  @property
  def args(self):
    return self._args

  @property
  def build_command(self):
    return self._build_command

  @property
  def start_dir(self):
    return self._start_dir

  @property
  def instr_dir(self):
    return self._instr_dir

  @property
  def process_launcher(self):
    return self._process_launcher

  def path_to(self, filename):
    if filename not in self._files:
      logger.error("Cannot find the file: %s", filename)
      return filename
    return self._files[filename]

  def __inspect_install_dir(self):
    files_in_dist = []
    scan_dir(self.args.dist, files_in_dist)

    if self.args.llvm_dir:
      logger.debug("LLVM DIR = %s", self.args.llvm_dir)
      scan_dir(self.args.llvm_dir, files_in_dist)

    for file in files_in_dist:
      filename = os.path.basename(file)
      if filename in self._files:
        logger.debug("Conflict with: %s", filename)
      self._files[filename] = os.path.realpath(file)

    print_files = False
    for expected in ('clang', LIB_CXX_ABI, RUNTIME_A, INSTRUMENT_TARGET, PINTOOL_TARGET):
      if expected not in self._files:
        logger.error("Cannot find %s" % expected)
        print_files = True

    if print_files:
      logger.info("All files:= %s", repr(self._files))


  def prepare_working_dir(self):
    """
      Creates the working directory where all instrumented files will be dumped.
    """
    if os.path.exists(self.args.instr_dir):
      self._instr_dir = clean_dir(self.args.instr_dir)
    else:
      self._instr_dir = build_dir(self.args.instr_dir)
    return self._instr_dir is not None


  def check_instrument(self):
    """
      Sanity check to make sure we have the right path were the instrumentation
      utils live.
    """
    instr_path = os.path.abspath(self.args.dist)
    if not os.path.isdir(instr_path):
      logger.error("The --dist must point to the directory containing the install")
      return False
    return True

  def remove_previous_files(self):
    models_file = os.path.join(os.curdir, MODELS_XXX)
    if os.path.exists(models_file):
      logger.debug("Delete previously generated models.xxx")
      os.remove(models_file)

  def monitor(self):
    if not self.check_instrument():
      return

    if not self.prepare_working_dir():
      logger.error("Cannot create the directory: %s", self.args.instr_dir)
      return

    self.remove_previous_files()

    self._process_launcher = ProcessLauncher(self)
    if not self.process_launcher.start():
      logger.error("Cannot launch the process...")
      return

    final_binary = self._process_launcher.final_binary if self._process_launcher.final_binary is not None else '$1'

    script_filename = os.path.join(self.instr_dir, 'run_fuzzer.sh')
    script = open(script_filename, 'w')
    script.write(RUN_FUZZER_SCRIPT % (os.path.dirname(self.path_to('coverage-fuzz')), self._process_launcher._simple_syms_dir, final_binary))
    script.close()

    st = os.stat(script_filename)
    os.chmod(script_filename, st.st_mode | stat.S_IEXEC)


def main():
  args, build_command = get_options()
  logging_level = logging.DEBUG if args.debug else logging.ERROR
  enableLogger(to_file=LOG_FILE, logging_level=logging_level)

  logger.info("Build command: %s", build_command)
  BuildWrapper(args, build_command).monitor()


if __name__ == "__main__":
  main()
