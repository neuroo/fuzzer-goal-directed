import React from 'react';
import { Route, IndexRoute } from 'react-router';

import App from './containers/App';
import ProgressContainer from './containers/Progress';
import CrashContainer from './containers/Crash';
import TestCaseContainer from './containers/TestCase';
import DebugContainer from './containers/Debug';
import WatcherContainer from './containers/Watcher';


export default (
  <Route path='/' component={App}>
    <IndexRoute component={ProgressContainer} />
    <Route path="/progress" component={ProgressContainer} />
    <Route path="/crashes" component={CrashContainer} />
    <Route path="/testcases" component={TestCaseContainer} />
    <Route path="/watchers" component={WatcherContainer} />
    <Route path="/debug" component={DebugContainer} />
    <Route path="*" component={ProgressContainer} />
  </Route>
)
