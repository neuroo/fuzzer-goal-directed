import { take, put, call, fork, race } from 'redux-saga'
import { history, socket } from '../services'
import * as actions from '../actions'

import _ from 'lodash';

function delay(millis) {
    return new Promise(resolve =>
      setTimeout(() => resolve(true), millis)
    )
}

function* watchNavigate() {
  while(true) {
    const {pathname} = yield take(actions.NAVIGATE)
    yield history.push(pathname)
  }
}


function* dispatchMessage(payload) {
  console.debug('dispatchMessage', payload);
  if (!payload.data)
    return;

  switch (payload.kind) {
    case 'progress':
      yield put(actions.updateElaspedTime(payload.data.elapsed));
      yield put(actions.updateGeneratedTestcases(payload.data.testcases));
      break;
    case 'score':
      yield put(actions.updateScores(payload.data));
      break;
    case 'global':
      yield put(actions.globalInformation(payload.data));
      break;
    case 'target':
      yield put(actions.targetInformation(payload.data));
      break;
    case 'coverage':
      yield put(actions.coverageInformation(payload.data));
      break;
    case 'entities':
      yield put(actions.entitiesInformation(payload.data));
      break;
    case 'crashers':
      yield put(actions.crashersInformation(payload.data));
      break;
    case 'crash':
      yield put(actions.crashInformation(payload.data));
      break;
    case 'debug-loop':
      yield put(actions.debugLoopInformation(payload.data));
      break;
    case 'stats':
      yield put(actions.statsInformation(payload.data));
      break;
    default:
      console.log("unhandled msg kind", payload.kind);
      break;
  }
}

function* watchSocketMessage() {
  while(true) {
    const { payload } = yield take(actions.SOCKET_MESSAGE);
    console.debug('watchSocketMessage', payload);
    if (payload) {
      yield call(dispatchMessage, payload);
    }
  }
}


function* dispatchMessageList(messages) {
  for (var i=0; i<messages.length; i++) {
    yield call(dispatchMessage, messages[i]);
  }
}


function* watchSocketMessageList() {
  while(true) {
    const { payload } = yield take(actions.SOCKET_MESSAGE_LIST);
    if (payload) {
      yield call(dispatchMessageList, payload);
    }
  }
}


function *sendMessageSocket(payload) {
  yield put(actions.socketSend(payload['kind'], payload['data']));
}


function* watchSocketSend() {
  while(true) {
    const { payload } = yield take(actions.SOCKET_SEND);
    if (payload) {
      yield fork(sendMessageSocket, payload);
    }
  }
}

export default function* root(getState) {
  yield fork(watchNavigate);
  yield fork(watchSocketSend);
  yield fork(watchSocketMessageList);
  yield fork(watchSocketMessage);
}
