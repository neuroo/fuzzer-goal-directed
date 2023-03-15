import React, { Component, PropTypes } from 'react';
import {connect} from 'react-redux';
import {bindActionCreators} from 'redux';
import ReactDOM from 'react-dom';

import _ from 'lodash';

import TextField from 'material-ui/lib/text-field';
import RaisedButton from 'material-ui/lib/raised-button';
import FlatButton from 'material-ui/lib/flat-button';
import FloatingActionButton from 'material-ui/lib/floating-action-button';
import ContentAdd from 'material-ui/lib/svg-icons/content/add';

import Tabs from 'material-ui/lib/tabs/tabs';
import Tab from 'material-ui/lib/tabs/tab';
import Paper from 'material-ui/lib/paper';

import List from 'material-ui/lib/lists/list';
import ListItem from 'material-ui/lib/lists/list-item';

import { socketSend } from '../actions';

class WatcherContainer extends Component {
  render() {
    return (
      <h1>WatcherContainer</h1>
    );
  }
}

WatcherContainer.propTypes = {
  watcher_info: PropTypes.object.isRequired,
  socketSend: PropTypes.func.isRequired,
  // Injected by React Router
  children: PropTypes.node
}


function mapStateToProps(state) {
  return {
    watcher_info: state.watcher_info,
  }
}

export default connect(mapStateToProps, {
  socketSend
})(WatcherContainer);
