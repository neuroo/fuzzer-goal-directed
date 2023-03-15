import { combineReducers } from 'redux';
import * as ActionTypes from '../actions';
import { history, socket } from '../services';

import _ from 'lodash';

// Updates error message to notify about the failed fetches.
function errorMessage(state = null, action) {
  const { type, error } = action

  if (type === ActionTypes.RESET_ERROR_MESSAGE) {
    return null
  } else if (error) {
    return action.error
  }

  return state
}

function router(state = { pathname: '/' }, action) {
  switch (action.type) {
    case ActionTypes.UPDATE_ROUTER_STATE:
      return action.state;
    default:
      return state;
  }
}


let initialPreviewDataState = {
  elapsed_time: 0,
  generated_testcases: 0,
  rates: []
};


function previewData(state = initialPreviewDataState, action) {
  const { type } = action;

  // XXX Safer if we do a deep-copy of the array...
  const updateRates = (state) => {
    if (state.elapsed_time > 0 && state.generated_testcases > 0) {
      state.rates = _.clone(state.rates);
      state.rates.push(state.generated_testcases / state.elapsed_time);
      if (state.rates.length > 25) {
        state.rates.shift();
      }
    }
    return state;
  }

  switch (type) {
    case ActionTypes.ELAPSED_TIME:
      return updateRates({...state, elapsed_time: action.elapsed_time});
    case ActionTypes.GENERATED_TESTCASES:
      return updateRates({...state, generated_testcases: action.generated_testcases});
    default:
      return state;
  }
}


let initialScoresState = {
  scores: []
};


function scores(state = initialScoresState, action) {
  const { type } = action;
  switch (type) {
    case ActionTypes.SCORES: {
      const new_scores = _.clone(state.scores);
      if (new_scores.length > 50) {
        new_scores.shift();
      }
      new_scores.push(action.scores);
      return {...state, scores: new_scores };
    }
    default:
      return state;
  }
}


function ws_message_handler(state=null, action) {
  const { type } = action;
  switch (type) {
    case ActionTypes.SOCKET_SEND:
      console.log('ws_message_handler', action.payload);
      socket.send(action.payload['kind'], action.payload['data']);
      break;
    default:
      break;
  }
  return null;
}

let initialGlobalInfoStore = {
  command: null,
  idir: null,
  options: {}
};

function global_info(state=initialGlobalInfoStore, action) {
  const { type } = action;
  switch (type) {
    case ActionTypes.GLOBAL_FUZZER_INFO:
      return {
        ...state,
        command: action.global_data.command,
        idir: action.global_data.idir,
        options: _.clone(action.global_data.options)
      }
    default:
      break;
  }
  return state;
}


let initialTargetInfoStore = {
  num_functions: 0,
  num_blocks: 0,
  num_goals: 0,
  elements: {}
};

function target_info(state=initialTargetInfoStore, action) {
  const { type } = action;
  switch (type) {
    case ActionTypes.GLOBAL_TARGET_INFO:
      return {
        ...state,
        num_functions: action.target_data.num_functions,
        num_blocks: action.target_data.num_blocks,
        num_goals: action.target_data.num_goals,
        elements: _.clone(action.target_data.elements)
      }
    default:
      break;
  }
  return state;
}


let initialCoverageInfoStore = {
  min: 0,
  max: 0,
  coverage: {}
};

function coverage_info(state=initialCoverageInfoStore, action) {
  const { type } = action;
  switch (type) {
    case ActionTypes.COVERAGE_INFO:
      var new_coverage = {};
      const scale_ratio = action.coverage_data.max;
      if (scale_ratio > 0) {
        _.forEach(action.coverage_data.coverage, (value, key) => {
          new_coverage[key] = (value) / (scale_ratio);
        });
      }
      else {
        _.forEach(action.coverage_data.coverage, (value, key) => {
          new_coverage[key] = value;
        });
      }

      return {
        ...state,
        min: action.coverage_data.min,
        max: action.coverage_data.max,
        coverage: new_coverage,
      }
    default:
      break;
  }
  return state;
}

let initialEntitiesInfoStore = {
  entities: []
};

function entities_info(state=initialEntitiesInfoStore, action) {
  const { type } = action;
  switch (type) {
    case ActionTypes.ALL_ENTITIES_INFO: {
      console.log('entities_info', action.entities_data)
      return {
        ...state,
        entities: _.clone(action.entities_data)
      }
    }
    default: break;
  }
  return state;
}


let initialCrashInfoStore = {
  crash_ids: {},
  crash: {}
};


function crash_info(state=initialCrashInfoStore, action) {
  const { type } = action;
  switch (type) {
    case ActionTypes.ALL_CRASHERS_INFO: {
      console.log('ALL_CRASHERS_INFO', action.crash_data)
      return {
        ...state,
        crash_ids: action.crash_data.crash_ids || {},
      }
    }
    case ActionTypes.CRASH_INFO: {
      console.log('CRASH_INFO', action.crash_data)
      if (!(action.crash_data.testcase_id in state.crash)) {
        var obj = {};
        obj[action.crash_data.testcase_id] = action.crash_data;
        return {
          ...state,
          crash: obj
        }
      }
    }
    default: break;
  }
  return state;
}

let initialCrashUISelectionStore = {
  selected_crash_id: null,
  selected_testcase_id: null
};


function crash_selection(state=initialCrashUISelectionStore, action) {
  const { type } = action;
  switch (type) {
    case ActionTypes.SELECT_CRASH_ID: {
      console.log('SELECT_CRASH_ID', action.crash_id)
      if (action.crash_id != state.selected_crash_id) {
        return {
          ...state,
          selected_crash_id: action.crash_id,
        }
      }
    }
    case ActionTypes.SELECT_TESTCASE_ID: {
      console.log('SELECT_TESTCASE_ID', action.testcase_id)
      if (action.testcase_id != state.selected_testcase_id) {
        return {
          ...state,
          selected_testcase_id: action.testcase_id,
        }
      }
    }
    default: break;
  }
  return state;
}


let initialDebugLoopStore = {
  input: '',
  trace: [],
  score: {
    goal: {
      absolute: 0,
      diff: 0,
      norm: 0
    },
    edge: {
      absolute: 0,
      diff: 0,
      norm: 0
    }
  }
};

function debug_info(state=initialDebugLoopStore, action) {
  const { type } = action;
  switch (type) {
    case ActionTypes.DEBUG_LOOP_INFO: {
      console.log('DEBUG_LOOP_INFO', action.debug_data)
      return {
        ...state,
        input: action.debug_data.input,
        trace: _.clone(action.debug_data.trace),
        score: _.clone(action.debug_data.score)
      };
    }
    default: break;
  }
  return state;
}

let statsInfoStore = {
  stats: {},
  best: []
};

function stats_info(state=initialDebugLoopStore, action) {
  const { type } = action;
  switch (type) {
    case ActionTypes.STATS_INFO: {
      console.log('STATS_INFO', action.stats_data)
      return {
        ...state,
        stats: action.stats_data.stats,
        best: _.clone(action.stats_data.best)
      };
    }
    default: break;
  }
  return state;
}

let hasCrashesInfoStore = false;

function has_crashes(state=hasCrashesInfoStore, action) {
  const { type } = action;
  switch (type) {
    case ActionTypes.ALL_CRASHERS_INFO: {
      return !_.isEmpty(action.crash_data.crash_ids);
    }
    default: break;
  }
  return state;
}

const rootReducer = combineReducers({
  previewData,
  scores,
  errorMessage,
  ws_message_handler,
  global_info,
  target_info,
  coverage_info,
  entities_info,
  debug_info,
  crash_info,
  stats_info,
  crash_selection,
  has_crashes,
  router
})

export default rootReducer
