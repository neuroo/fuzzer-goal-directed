import React, { Component, PropTypes } from 'react';
import {connect} from 'react-redux';
import {bindActionCreators} from 'redux';
import ReactDOM from 'react-dom';

import _ from 'lodash';


export default class TextLines extends Component {
  render() {
    var lines = this.props.lines;
    var br = lines.map((line) => {
      return (<span style={{whiteSpace:'pre'}}>{ line }<br/></span>);
    });
    return (<div>{ br }</div>);
  }
}
