import React, { Component, PropTypes } from 'react';
import { connect } from 'react-redux';
import { socketSend } from '../actions';

class WebSocketComponent extends Component {
  constructor(props) {
    super(props);
    this.state = {
      ws: null
    };
  }

  log(logline) {
    if (this.props.debug === true) {
      console.log(logline);
    }
  }

  componentWillMount() {
    this.log('Websocket componentWillMount');
    this.state.ws = new WebSocket(this.props.url);
    var self = this;
    var ws = this.state.ws;
    ws.addEventListener('open', () => {
      self.log('Websocket connected');
    });
    ws.addEventListener('message', (event) => {
      var data = JSON.parse(event.data);
      self.log('Websocket incoming data');
      self.log(event.data);
      self.props.onMessage(data);
    });
    ws.addEventListener('close', () => {
      self.log('Websocket disconnected');
    });
  }

  componentWillUnmount() {
    this.log('Websocket componentWillUnmount');
    this.state.ws.close();
  }

  render() {
    const { url, onMessage } = this.props;
    return React.createElement("div", React.__spread({}, this.props))
  }
};

WebSocketComponent.propTypes = {
  url: PropTypes.string.isRequired,
  onMessage: PropTypes.func.isRequired,
  debug: PropTypes.bool
};

function mapStateToProps(state, ownProp) {
  return {
    url: ownProp.url,
    onMessage: ownProp.onMessage,
    debug: ownProp.debug
  }
}

export default connect(mapStateToProps, {
  socketSend
})(WebSocketComponent);
