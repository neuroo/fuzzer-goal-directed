export const NAVIGATE =  'NAVIGATE';
export const UPDATE_ROUTER_STATE = 'UPDATE_ROUTER_STATE';
export const RESET_ERROR_MESSAGE = 'RESET_ERROR_MESSAGE';

export const SOCKET_MESSAGE = 'SOCKET_MESSAGE';
export const SOCKET_MESSAGE_LIST = 'SOCKET_MESSAGE_LIST';
export const SOCKET_SEND = 'SOCKET_SEND';

export const ELAPSED_TIME = 'ELAPSED_TIME';
export const GENERATED_TESTCASES = 'GENERATED_TESTCASES';
export const SCORES = 'SCORES';
export const GLOBAL_FUZZER_INFO = 'GLOBAL_FUZZER_INFO';
export const GLOBAL_TARGET_INFO = 'GLOBAL_TARGET_INFO';
export const COVERAGE_INFO = 'COVERAGE_INFO';
export const ALL_ENTITIES_INFO = 'ALL_ENTITIES_INFO';
export const ALL_CRASHERS_INFO = 'ALL_CRASHERS_INFO';
export const CRASH_INFO = 'CRASH_INFO';
export const SELECT_CRASH_ID = 'SELECT_CRASH_ID';
export const SELECT_TESTCASE_ID = 'SELECT_TESTCASE_ID';
export const DEBUG_LOOP_INFO = 'DEBUG_LOOP_INFO';
export const STATS_INFO = 'STATS_INFO';

function action(type, payload = {}) {
  return {type, ...payload};
}

export const updateRouterState = state => action(UPDATE_ROUTER_STATE, {state});
export const navigate = pathname => action(NAVIGATE, {pathname});
export const resetErrorMessage = () => action(RESET_ERROR_MESSAGE);

export const socketMessage = (kind, data) => action(SOCKET_MESSAGE, {payload: {kind, data}});

export const socketMessageList = msg => action(SOCKET_MESSAGE_LIST, {payload: msg});

export const socketSend = (kind, data) => action(SOCKET_SEND, {payload: {kind, data}});

export const updateElaspedTime = elapsed_time => action(ELAPSED_TIME, {elapsed_time});
export const updateGeneratedTestcases = generated_testcases => action(GENERATED_TESTCASES, {generated_testcases});
export const updateScores = scores => action(SCORES, {scores});
export const globalInformation = global_data => action(GLOBAL_FUZZER_INFO, {global_data});
export const targetInformation = target_data => action(GLOBAL_TARGET_INFO, {target_data});
export const coverageInformation = coverage_data => action(COVERAGE_INFO, {coverage_data});
export const entitiesInformation = entities_data => action(ALL_ENTITIES_INFO, {entities_data});
export const crashersInformation = crash_data => action(ALL_CRASHERS_INFO, {crash_data});
export const crashInformation = crash_data => action(CRASH_INFO, {crash_data});
export const selectCrashId = crash_id => action(SELECT_CRASH_ID, {crash_id});
export const selectTestCaseId = testcase_id => action(SELECT_TESTCASE_ID, {testcase_id});
export const debugLoopInformation = debug_data => action(DEBUG_LOOP_INFO, {debug_data});
export const statsInformation = stats_data => action(STATS_INFO, {stats_data});
