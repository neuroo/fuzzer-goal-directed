import os
import sys
import multiprocessing
from  multiprocessing import Process
import zmq
import random

PATH_INTERPROCESS = 'ipc:///tmp/test_pyzmq_001'

def publisher():
  consumer_id = random.randrange(1,10005)
  context = zmq.Context()

  consumer_sender = context.socket(zmq.PUSH)
  consumer_sender.connect(PATH_INTERPROCESS)
  consumer_data = 0

  while True:
    result = { 'consumer' : consumer_id, 'num' : random.randrange(1,10005)}
    consumer_sender.send_json(result)
    consumer_data += 1
    print 'publisher', consumer_id, consumer_data


def receiver():
    context = zmq.Context()
    results_receiver = context.socket(zmq.PULL)
    results_receiver.bind(PATH_INTERPROCESS)
    collecter_data = 0
    while True:
      result = results_receiver.recv_json()
      collecter_data += 1
      print 'receiver', collecter_data, result


def main():
  """
    Simulates how the fuzzer works
  """
  Process(target=publisher).start()
  Process(target=publisher).start()
  Process(target=publisher).start()
  Process(target=publisher).start()

  Process(target=receiver).start()


if __name__ == '__main__':
  main()
