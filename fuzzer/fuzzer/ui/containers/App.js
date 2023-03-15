import React, { Component, PropTypes } from 'react';
import { connect } from 'react-redux';
import { navigate, updateRouterState, resetErrorMessage,
         socketMessage, socketMessageList, socketSend } from '../actions';

import _ from 'lodash';

import ToolBar from 'material-ui/lib/toolbar/toolbar';
import ToolbarGroup from 'material-ui/lib/toolbar/toolbar-group';
import ToolbarSeparator from 'material-ui/lib/toolbar/toolbar-separator';
import ToolbarTitle from 'material-ui/lib/toolbar/toolbar-title';

import Badge from 'material-ui/lib/badge';

import FontIcon from 'material-ui/lib/font-icon';
import RaisedButton from 'material-ui/lib/raised-button';
import FlatButton from 'material-ui/lib/flat-button';

import WebSocket from '../components/socket';
import UpdatingPreviewContainer from '../containers/UpdatingPreview';



class MenuItem extends Component {
  render() {
    var menu_item;
    if (this.props.history.isActive(this.props.target_url)) {
      menu_item = <RaisedButton label={this.props.label} primary={true} key={this.props.target_url}
                    onClick={() => this.props.handle_change(this.props.target_url)}>
                  </RaisedButton>

    }
    else {
      menu_item = <FlatButton label={this.props.label} secondary={true} key={this.props.target_url}
                  onClick={() => this.props.handle_change(this.props.target_url)} disabled={this.props.disabled}>
                 </FlatButton>
    }
    return menu_item;
  }
}




class App extends Component {
  constructor(props) {
    super(props);
    this.handleChange = this.handleChange.bind(this);
    this.handleDismissClick = this.handleDismissClick.bind(this);
    this.handleSocketMessage = this.handleSocketMessage.bind(this);
  }

  componentWillMount() {
    this.props.updateRouterState({
      pathname: this.props.location.pathname,
      params  : this.props.params
    });
  }

  componentDidMount() {
    this.handleChange('progress');
    this.props.socketSend('global');
    this.props.socketSend('target');
    this.props.socketSend('coverage');
  }

  componentWillReceiveProps(nextProps) {
    if(this.props.location.pathname !== nextProps.location.pathname)
      this.props.updateRouterState({
        pathname: nextProps.location.pathname,
        params  : nextProps.params
      });
  }

  handleDismissClick(e) {
    this.props.resetErrorMessage();
    e.preventDefault();
  }

  handleChange(nextValue) {
    this.props.navigate(`/${nextValue}`);
  }

  handleSocketMessage(data) {
    const { socketMessage, socketMessageList } = this.props
    if (Array.isArray(data)) {
      socketMessageList(data);
    } else {
      socketMessage(data['kind'], data['data']);
    }
  }

  renderErrorMessage() {
    const { errorMessage } = this.props
    if (!errorMessage) {
      return null
    }

    return (
      <p style={{ backgroundColor: '#e99', padding: 10 }}>
        <b>{errorMessage}</b>
        {' '}
        (<a href="#"
            onClick={this.handleDismissClick}>
          Dismiss
        </a>)
      </p>
    )
  }

  render() {
    const { children, inputValue, previewData } = this.props;
    const url = window.ws_address;
    const has_no_crash = !this.props.has_crashes;

    return (
      <div>
        <WebSocket ref="websocket" debug={true} url={url}
                   onMessage={this.handleSocketMessage} />

        <ToolBar style={{ backgroundColor: '#fff',
                          borderBottom: '3px solid #efefef'}}>

          <ToolbarGroup firstChild={true} float="left" style={{ marginTop: '10px',
                                                                marginLeft: '10px' }}>
            <MenuItem history={this.props.history} label="coverage-fuzz"
                      handle_change={this.handleChange} target_url={'progress'} disabled={false} />

            <MenuItem history={this.props.history} label="Crashes"
                      handle_change={this.handleChange} target_url={'crashes'} disabled={has_no_crash} />

            <MenuItem history={this.props.history} label="Testcases"
                      handle_change={this.handleChange} target_url={'testcases'} disabled={false} />

            <MenuItem history={this.props.history} label="Watcher"
              handle_change={this.handleChange} target_url={'watchers'} disabled={false} />

            <MenuItem history={this.props.history} label="Debug"
                      handle_change={this.handleChange} target_url={'debug'} disabled={false} />

          </ToolbarGroup>
          <UpdatingPreviewContainer data={previewData}/>
        </ToolBar>
        {children}
      </div>
    )
  }
}

App.propTypes = {
  has_crashes: PropTypes.bool.isRequired,
  crash_info: PropTypes.object.isRequired,
  previewData: PropTypes.object.isRequired,
  errorMessage: PropTypes.string,
  inputValue: PropTypes.string.isRequired,
  navigate: PropTypes.func.isRequired,
  updateRouterState: PropTypes.func.isRequired,
  resetErrorMessage: PropTypes.func.isRequired,
  socketMessage: PropTypes.func.isRequired,
  socketMessageList: PropTypes.func.isRequired,
  socketSend: PropTypes.func.isRequired,

  // Injected by React Router
  children: PropTypes.node
}

function mapStateToProps(state) {
  return {
    has_crashes: state.has_crashes,
    crash_info: state.crash_info,
    errorMessage: state.errorMessage,
    inputValue: state.router.pathname.substring(1),
    previewData: state.previewData,
  }
}

export default connect(mapStateToProps, {
  navigate,
  updateRouterState,
  resetErrorMessage,
  socketMessageList,
  socketMessage,
  socketSend
})(App)
