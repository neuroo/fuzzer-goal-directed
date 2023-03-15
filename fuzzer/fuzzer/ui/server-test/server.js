var express = require('express');
var app = express();
var expressWs = require('express-ws')(app);
var utils = require('util');


var current_elapsed = 0, current_testcases = 0,
    current_min_abs = 0, current_min_diff = 0,
    current_max_abs = 0, current_max_diff = 0;

function randomInt(low, high) {
    return Math.floor(Math.random() * (high - low) + low);
}

function create_message(put_crashers) {
  current_elapsed += .5;
  current_testcases += randomInt(40, 60);
  current_min_abs += randomInt(0, 2);
  current_min_diff += randomInt(0, 2);
  current_max_abs += randomInt(0, 2);
  current_max_diff += randomInt(0, 2);

  return [
    {error : false, kind : "progress", data : {elapsed : current_elapsed, testcases : current_testcases}},
    {error : false, kind : "score", data : {
      index: current_testcases,
      min : {absolute: current_min_abs, diff: current_min_diff},
      max : {absolute: current_max_abs, diff: current_max_diff}}
    },
    create_coverage_message(),
    create_crashers_message(),
    create_stats_message()
  ]
}

function create_global_message() {
  return {
    error: false,
    kind: 'global',
    data: {
      command: './run_foo __INPUT__',
      idir: './fuzz_idir',
      options: {
        woot_opt_1: "true",
        woot_opt_2: "false"
      }
    }
  }
}

function create_null_message(kind) {
  return {
    error: true,
    kind: kind,
    data: null
  }
}

function create_target_message() {
  return {
    error: false,
    kind: 'target',
    data: {
      num_functions: 4,
      num_blocks: 42,
      num_goals: 16,
      elements: {
        0: "source",
        1: "function",
        2: "block",
        3: "block",
        4: "goal"
      }
    }
  }
}

// Initial coverage
var COVERAGE = {
  "11":0,"12":0,"13":0,"14":0,"15":0,"16":0,"18":0,
  "2":0,"20":0,"21":0,"22":0,"25":0,"28":0,"3":0,"30":0,
  "31":0,"32":0,"33":0,"34":0,"35":0,"36":0,"39":0,"4":0,
  "40":0,"43":0,"44":0,"45":0,"6":0,"7":0,"8":0,"9":0
};


function create_coverage_message() {
  var keys = Object.keys(COVERAGE);
  var max_value = COVERAGE[keys[0]],
      min_value = COVERAGE[keys[0]];

  for (var i=0; i<keys.length; i++) {
    var key = keys[i];
    if (randomInt(0, 10) == 0) {
      COVERAGE[key] += randomInt(1, 100);
    }
    if (COVERAGE[key] < min_value) min_value = COVERAGE[key];
    if (COVERAGE[key] > max_value) max_value = COVERAGE[key];
  }

  return {"data":
    {
      "coverage": COVERAGE,
      "max": max_value,
      "min": min_value
    },
    "error":false,
    "kind":"coverage"
  }
}


function create_entities_message() {
  return {
    "data": [
      {
        "data": {
          "blocks": [
            3, 4, 6
          ],
          "id": 2,
          "name": "trigger_fault"
        },
        "error": false,
        "kind": "element_function"
      },
      {
        "data": {
          "blocks": [
            8,
            9,
            11
          ],
          "id": 7,
          "name": "is_b_next"
        },
        "error": false,
        "kind": "element_function"
      },
      {
        "data": {
          "blocks": [
            13,            14,            15,            16,            18,            20,
            21,            22,            25,            28,            30,            31
          ],
          "id": 12,
          "name": "check_string"
        },
        "error": false,
        "kind": "element_function"
      },
      {
        "data": {
          "blocks": [
            33,            34,            35,            36,            39,
            40,            43,            44,            45
          ],
          "id": 32,
          "name": "main"
        },
        "error": false,
        "kind": "element_function"
      }
    ],
    "error": false,
    "kind": "entities"
  }
}


function create_crashers_message() {
  return {
    "data": {
      "crash_ids": {
        "62eb008d2039bc0e6a3bf108645cca57" : [94],
        "912b619d2cc507e3fc8f685780e3a216" : [
          3230,
          3363,
          3377,
          3396,
          3413,
          3551,
          3644,
          3654,
          3660,
          3661,
          3662,
          3670
        ]
      }
    },
    "error": false,
    "kind": "crashers"
  }
}

var STATIC_CRASHES = {
  94: {
    "data": {
      "crash_id": "62eb008d2039bc0e6a3bf108645cca57",
      "info": {
        "crash_file": "AF972E06-D3B6-41E0-BF15-177C2AFF2FCE.dmp",
        "crash_info": {
          "address": "0x57585000",
          "assertion": "",
          "crashed": true,
          "exploitability": 5,
          "reason": "EXC_BAD_ACCESS / KERN_INVALID_ADDRESS",
          "thread": 0,
          "time": 1455713180
        },
        "testcase_id": 94,
        "trace": [
          {
            "file": "/Users/<path-to>/examples/synthetic/ab_crasher/src/instr_idir/./ab_fuzzer",
            "file_base_address": "0x10867b000",
            "file_identifier": "id",
            "frame": 0,
            "function_base": "0x10867bc32",
            "function_name": "__Z12check_stringPKcm",
            "offset": "0x10867bc32",
            "registers": {
              "r10": "0x0000000000000003",
              "r11": "0x00007fc051d00000",
              "r12": "0x0000000000000994",
              "r13": "0x0000000000000005",
              "r14": "0xffffffffffffffff",
              "r15": "0x00007fff5758466c",
              "r8": "0x00007fff575843c0",
              "r9": "0x00000000ffffff00",
              "rax": "0x0000000000000000",
              "rbp": "0x00007fff57584470",
              "rbx": "0x0000000000000994",
              "rcx": "0x0000000000000000",
              "rdi": "0x0013600000136103",
              "rdx": "0x000000010869f8a8",
              "rip": "0x000000010867bc32",
              "rsi": "0x0013610000136100",
              "rsp": "0x00007fff57584440"
            },
            "source_file_name": "",
            "source_line": 0,
            "source_line_base": 0,
            "trust": "given as instruction pointer in context"
          },
          {
            "file": "/Users/<path-to>/examples/synthetic/ab_crasher/src/instr_idir/./ab_fuzzer",
            "file_base_address": "0x10867b000",
            "file_identifier": "id",
            "frame": 1,
            "function_base": "0x10867bedb",
            "function_name": "_main",
            "offset": "0x10867bedb",
            "registers": {
              "rbp": "0x00007fff575844c0",
              "rip": "0x000000010867bedc",
              "rsp": "0x00007fff57584480"
            },
            "source_file_name": "",
            "source_line": 0,
            "source_line_base": 0,
            "trust": "stack scanning"
          },
          {
            "file": "/usr/lib/system/libdyld.dylib",
            "file_base_address": "0x7fff943af000",
            "file_identifier": "id",
            "frame": 2,
            "function_base": "0x7fff943b25c8",
            "function_name": "",
            "offset": "0x7fff943b25c8",
            "registers": {
              "rbp": "0x00007fff575844d8",
              "rip": "0x00007fff943b25c9",
              "rsp": "0x00007fff575844d0"
            },
            "source_file_name": "",
            "source_line": 0,
            "source_line_base": 0,
            "trust": "stack scanning"
          },
          {
            "file": "/usr/lib/system/libdyld.dylib",
            "file_base_address": "0x7fff943af000",
            "file_identifier": "id",
            "frame": 3,
            "function_base": "0x7fff943b25c8",
            "function_name": "",
            "offset": "0x7fff943b25c8",
            "registers": {
              "rbp": "0x00007fff575844d8",
              "rip": "0x00007fff943b25c9",
              "rsp": "0x00007fff575844d8"
            },
            "source_file_name": "",
            "source_line": 0,
            "source_line_base": 0,
            "trust": "stack scanning"
          }
        ]
      },
      "testcase": {
        "buffer": "",
        "length": 19,
        "repr": [
          "0000 : .@cL2&6Y.0....2. 00 40 63 4C 32 26 36 59 CD 30 15 89 B1 96 32 A8 ",
          "0010 : 8u.              38 75 BC ",
          ""
        ]
      },
      "testcase_id": 94
    },
    "error": false,
    "kind": "crash"
  },
  3230 : {
    "data": {
      "crash_id": "62eb008d2039bc0e6a3bf108645cca57",
      "info": {
        "crash_file": "9BBA0E7A-FB59-47AC-8EB2-5D9C1C0AC93C.dmp",
        "crash_info": {
          "address": "0x5e5c9000",
          "assertion": "",
          "crashed": true,
          "exploitability": 5,
          "reason": "EXC_BAD_ACCESS / KERN_INVALID_ADDRESS",
          "thread": 0,
          "time": 1455713202
        },
        "testcase_id": 3230,
        "trace": [
          {
            "file": "/Users/<path-to>/examples/synthetic/ab_crasher/src/instr_idir/./ab_fuzzer",
            "file_base_address": "0x101637000",
            "file_identifier": "id",
            "frame": 0,
            "function_base": "0x101637c32",
            "function_name": "__Z12check_stringPKcm",
            "offset": "0x101637c32",
            "registers": {
              "r10": "0x0000000000000003",
              "r11": "0x00007ff0c8e00000",
              "r12": "0x0000000000000994",
              "r13": "0x0000000000000005",
              "r14": "0xffffffffffffffff",
              "r15": "0x00007fff5e5c866c",
              "r8": "0x00007fff5e5c83c0",
              "r9": "0x00000000ffffff00",
              "rax": "0x0000000000000000",
              "rbp": "0x00007fff5e5c8470",
              "rbx": "0x0000000000000994",
              "rcx": "0x0000000000000000",
              "rdi": "0x0013600000136103",
              "rdx": "0x000000010165b8a8",
              "rip": "0x0000000101637c32",
              "rsi": "0x0013610000136100",
              "rsp": "0x00007fff5e5c8440"
            },
            "source_file_name": "",
            "source_line": 0,
            "source_line_base": 0,
            "trust": "given as instruction pointer in context"
          },
          {
            "file": "/Users/<path-to>/examples/synthetic/ab_crasher/src/instr_idir/./ab_fuzzer",
            "file_base_address": "0x101637000",
            "file_identifier": "id",
            "frame": 1,
            "function_base": "0x101637edb",
            "function_name": "_main",
            "offset": "0x101637edb",
            "registers": {
              "rbp": "0x00007fff5e5c84c0",
              "rip": "0x0000000101637edc",
              "rsp": "0x00007fff5e5c8480"
            },
            "source_file_name": "",
            "source_line": 0,
            "source_line_base": 0,
            "trust": "stack scanning"
          },
          {
            "file": "/usr/lib/system/libdyld.dylib",
            "file_base_address": "0x7fff943af000",
            "file_identifier": "id",
            "frame": 2,
            "function_base": "0x7fff943b25c8",
            "function_name": "",
            "offset": "0x7fff943b25c8",
            "registers": {
              "rbp": "0x00007fff5e5c84d8",
              "rip": "0x00007fff943b25c9",
              "rsp": "0x00007fff5e5c84d0"
            },
            "source_file_name": "",
            "source_line": 0,
            "source_line_base": 0,
            "trust": "stack scanning"
          },
          {
            "file": "/usr/lib/system/libdyld.dylib",
            "file_base_address": "0x7fff943af000",
            "file_identifier": "id",
            "frame": 3,
            "function_base": "0x7fff943b25c8",
            "function_name": "",
            "offset": "0x7fff943b25c8",
            "registers": {
              "rbp": "0x00007fff5e5c84d8",
              "rip": "0x00007fff943b25c9",
              "rsp": "0x00007fff5e5c84d8"
            },
            "source_file_name": "",
            "source_line": 0,
            "source_line_base": 0,
            "trust": "stack scanning"
          }
        ]
      },
      "testcase": {
        "buffer": "",
        "length": 17,
        "repr": [
          "0000 : .{ t...&...V..K{ 00 7B 20 74 DD B8 D6 26 99 8C FD 56 BC 09 4B 7B ",
          "0010 : .                0C ",
          ""
        ]
      },
      "testcase_id": 3230
    },
    "error": false,
    "kind": "crash"
  }
}


function create_crash_message(id) {
  try {
    return STATIC_CRASHES[id];
  }
  catch (e) {
    return create_null_message('crash');
  }
}

function create_debug_loop_message() {
  return {
    "data": {
      "input":"ABABAB",
      "score": {
        "edge": {"absolute":15,"diff":0},
        "goal":{"absolute":19,"diff":5}
      },
      "trace":[18,25,22,18,25,22,18,25,22]
    },
    "error":false,
    "kind":"debug-loop"
  }
}

function create_stats_message() {
  return   {
    "data": {
      "best": [
        {
          "length": 63,
          "position": 1,
          "repr": [
            "0000 : ......\"V.E..da.. CE 95 C9 F8 06 B8 22 56 95 45 A1 E6 64 61 04 CD ",
            "0010 : E..da..E..da..E. 45 A1 E6 64 61 04 CD 45 A1 E6 64 61 04 CD 45 A1 ",
            "0020 : .da..E..da..zE.. E6 64 61 04 CD 45 A1 E6 64 61 04 CD 7A 45 E1 BB ",
            "0030 : Y...R ..Ds..}.x  59 1A E3 DA 52 20 0C CE 44 73 83 A7 7D 98 78 ",
            ""
          ],
          "score": {
            "edge": {
              "absolute": 62,
              "diff": 0,
              "norm": 62
            },
            "goal": {
              "absolute": 128,
              "diff": 0,
              "norm": 128
            }
          }
        },
        {
          "length": 63,
          "position": 2,
          "repr": [
            "0000 : ......\"V.E..da.. CE 95 C9 F8 06 B8 22 56 95 45 A1 E6 64 61 04 CD ",
            "0010 : E..da..E..da..E. 45 A1 E6 64 61 04 CD 45 A1 E6 64 61 04 CD 45 A1 ",
            "0020 : .da..E..da..zE.. E6 64 61 04 CD 45 A1 E6 64 61 04 CD 7A 45 E1 BB ",
            "0030 : Y...R ..Ds..}.x  59 1A E3 DA 52 20 0C CE 44 73 83 A7 7D 98 78 ",
            ""
          ],
          "score": {
            "edge": {
              "absolute": 62,
              "diff": 0,
              "norm": 62
            },
            "goal": {
              "absolute": 128,
              "diff": 0,
              "norm": 128
            }
          }
        },
        {
          "length": 63,
          "position": 3,
          "repr": [
            "0000 : ......\"V.E..da.. CE 95 C9 F8 06 B8 22 56 95 45 A1 E6 64 61 04 CD ",
            "0010 : E..da..E..da..E. 45 A1 E6 64 61 04 CD 45 A1 E6 64 61 04 CD 45 A1 ",
            "0020 : .da..E..da..zE.. E6 64 61 04 CD 45 A1 E6 64 61 04 CD 7A 45 E1 BB ",
            "0030 : Y...R ..Ds..}.x  59 1A E3 DA 52 20 0C CE 44 73 83 A7 7D 98 78 ",
            ""
          ],
          "score": {
            "edge": {
              "absolute": 62,
              "diff": 0,
              "norm": 62
            },
            "goal": {
              "absolute": 128,
              "diff": 0,
              "norm": 128
            }
          }
        },
        {
          "length": 63,
          "position": 4,
          "repr": [
            "0000 : ......\"V.E..da.. CE 95 C9 F8 06 B8 22 56 95 45 A1 E6 64 61 04 CD ",
            "0010 : E..da..E..da..E. 45 A1 E6 64 61 04 CD 45 A1 E6 64 61 04 CD 45 A1 ",
            "0020 : .da..E..da..zE.. E6 64 61 04 CD 45 A1 E6 64 61 04 CD 7A 45 E1 BB ",
            "0030 : Y...R ..Ds..}.x  59 1A E3 DA 52 20 0C CE 44 73 83 A7 7D 98 78 ",
            ""
          ],
          "score": {
            "edge": {
              "absolute": 62,
              "diff": 0,
              "norm": 62
            },
            "goal": {
              "absolute": 128,
              "diff": 0,
              "norm": 128
            }
          }
        },
        {
          "length": 63,
          "position": 5,
          "repr": [
            "0000 : ......\"V.E..da.. CE 95 C9 F8 06 B8 22 56 95 45 A1 E6 64 61 04 CD ",
            "0010 : E..da..E..da..E. 45 A1 E6 64 61 04 CD 45 A1 E6 64 61 04 CD 45 A1 ",
            "0020 : .da..E..da..zE.. E6 64 61 04 CD 45 A1 E6 64 61 04 CD 7A 45 E1 BB ",
            "0030 : Y...R ..Ds..}.x  59 1A E3 DA 52 20 0C CE 44 73 83 A7 7D 98 78 ",
            ""
          ],
          "score": {
            "edge": {
              "absolute": 62,
              "diff": 0,
              "norm": 62
            },
            "goal": {
              "absolute": 128,
              "diff": 0,
              "norm": 128
            }
          }
        },
        {
          "length": 60,
          "position": 6,
          "repr": [
            "0000 : ..F.p...o#..VdwH F1 B3 46 F2 70 A8 F1 B3 6F 23 1F 1C 56 64 77 48 ",
            "0010 : c....b~....rJ~.. 63 A8 D7 04 19 62 7E 8E A9 FD 05 72 4A 7E 8E A9 ",
            "0020 : ..rJ~....rJ~.... FD 05 72 4A 7E 8E A9 FD 05 72 4A 7E 8E A9 FD 05 ",
            "0030 : rJ~....rJ5..     72 4A 7E 8E A9 FD 05 72 4A 35 05 AE ",
            ""
          ],
          "score": {
            "edge": {
              "absolute": 59,
              "diff": 0,
              "norm": 59
            },
            "goal": {
              "absolute": 122,
              "diff": 0,
              "norm": 122
            }
          }
        },
        {
          "length": 58,
          "position": 7,
          "repr": [
            "0000 : .Pm.....1....5.. 0D 50 6D D2 B5 01 B2 8C 31 9C E0 A6 06 35 06 81 ",
            "0010 : .f....t(9.....&. B4 66 E4 88 CF 10 74 28 39 0C 83 F8 A1 91 26 E8 ",
            "0020 : .TTTTT.'Q..O..s> 0B 54 54 54 54 54 B9 27 51 91 EE 4F 12 94 73 3E ",
            "0030 : Y..P.k4.q.       59 AC E2 50 EC 6B 34 AF 71 B4 ",
            ""
          ],
          "score": {
            "edge": {
              "absolute": 57,
              "diff": 0,
              "norm": 57
            },
            "goal": {
              "absolute": 118,
              "diff": 0,
              "norm": 118
            }
          }
        },
        {
          "length": 54,
          "position": 8,
          "repr": [
            "0000 : .......:A.>.L... 9C A8 A6 98 DA F6 BD 3A 41 0D 3E DB 4C F7 8E E4 ",
            "0010 : .rf....@t..|U. . FB 72 66 12 BF F4 E2 40 74 94 A9 7C 55 AA 20 A8 ",
            "0020 : Tv.+o26.9..&N... 54 76 13 2B 6F 32 36 EC 39 0D B2 26 4E 1D F0 E0 ",
            "0030 : .&...,           0B 26 87 F2 EB 2C ",
            ""
          ],
          "score": {
            "edge": {
              "absolute": 55,
              "diff": 0,
              "norm": 55
            },
            "goal": {
              "absolute": 112,
              "diff": 0,
              "norm": 112
            }
          }
        },
        {
          "length": 54,
          "position": 9,
          "repr": [
            "0000 : :q..F....~.wW... 3A 71 B8 17 46 D5 D6 C7 CA 7E 92 77 57 8A F4 95 ",
            "0010 : w..bR...U..?T.A. 77 81 01 62 52 10 CE F7 55 AA 93 3F 54 E6 41 A9 ",
            "0020 : -.k.n.c.$r.0...v 2D DC 6B CB 6E 81 63 2E 24 72 D9 30 EB A5 8A 76 ",
            "0030 : )$'..[           29 24 27 D4 BE 5B ",
            ""
          ],
          "score": {
            "edge": {
              "absolute": 55,
              "diff": 0,
              "norm": 55
            },
            "goal": {
              "absolute": 112,
              "diff": 0,
              "norm": 112
            }
          }
        },
        {
          "length": 54,
          "position": 10,
          "repr": [
            "0000 : .......:A.>.L... 9C A8 A6 98 DA F6 BD 3A 41 0D 3E DB 4C F7 8E E4 ",
            "0010 : .rf....@T..|U. . FB 72 66 12 BF F4 E2 40 54 94 A9 7C 55 AA 20 A8 ",
            "0020 : Tv.+o26.9..&N... 54 76 13 2B 6F 32 36 EC 39 0D B2 26 4E 1D F0 E0 ",
            "0030 : .&...,           0B 26 87 F2 EB 2C ",
            ""
          ],
          "score": {
            "edge": {
              "absolute": 55,
              "diff": 0,
              "norm": 55
            },
            "goal": {
              "absolute": 112,
              "diff": 0,
              "norm": 112
            }
          }
        }
      ],
      "stats": {
        "Average Size": Math.random(),
        "Letter Density Size": Math.random()
      }
    },
    "error": false,
    "kind": "stats"
  };
}


// pre-def of the websocket handler, a global var
var aWss;

function dispatchMessage(msg) {
  const obj = JSON.parse(msg);
  aWss.clients.forEach((client) => {
    switch (obj.kind) {
      case 'global':
        client.send(JSON.stringify(create_global_message()));
        break;
      case 'target':
        client.send(JSON.stringify(create_target_message()));
        break;
      case 'coverage':
        client.send(JSON.stringify(create_coverage_message()));
        break;
      case 'functions':
        client.send(JSON.stringify(create_entities_message()));
        break;
      case 'crashers':
        client.send(JSON.stringify(create_crashers_message()));
        break;
      case 'crash':
        var id = obj.data.id;
        client.send(JSON.stringify(create_crash_message(id)));
        break;
      case 'debug-loop':
        var id = obj.data.id;
        client.send(JSON.stringify(create_debug_loop_message(id)));
        break;
      default: break;
    }
  });
}


app.use((req, res, next) => {
  req.testing = 'testing';
  return next();
});

app.get('/', (req, res, next) => {
  res.end();
});

app.ws('/', (ws, req) => {
  ws.on('message', (msg) => dispatchMessage(msg));
});

aWss = expressWs.getWss('/');

setInterval(() => {
  aWss.clients.forEach((client) => {
    client.send(JSON.stringify(create_message()));
  });
}, 1000);

app.listen(9999);
