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


class ResultInfo extends Component {
  render() {
    const { input, trace, score } = this.props;
    var trace_list = [];
    if (trace) {
      var num_trace_elements = trace.length;
      for (var i=1; i<num_trace_elements; i++) {
        var text = '' + trace[i - 1] + ' -> ' + trace[i];
        var key_text = 'trace_' + i + ':' + text;
        trace_list.push(
          <ListItem key={key_text} primaryText={text}/>
        );
      }
    }

    return (
      <div className="container">
        <div className="row">
          <br />
          <Paper zDepth={1} style={{ padding: '5px', width: '100%',
                                    fontFamily:'monospace', fontSize: '14px', border: '0' }}>
            <div className="chip">Input</div>
            {' '}{input}
          </Paper>
          <br />
          <Paper zDepth={1} style={{ padding: '5px', width: '100%' }}>
            <table>
              <thead>
                <tr>
                    <th>Dimension</th>
                    <th>Absolute</th>
                    <th>Improvment</th>
                    <th>Norm</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>Coverage</td>
                  <td>{score.edge.absolute}</td>
                  <td>{score.edge.diff}</td>
                  <td>{score.edge.norm}</td>
                </tr>
                <tr>
                  <td>Derived Goals</td>
                  <td>{score.goal.absolute}</td>
                  <td>{score.goal.diff}</td>
                  <td>{score.goal.norm}</td>
                </tr>
              </tbody>
            </table>
          </Paper>
          <br />
          <Paper zDepth={1} style={{ padding: '5px', width: '100%' }}>
            <List>
              { trace_list }
            </List>
          </Paper>
        </div>
      </div>
    );
  }
}


class DebugContainer extends Component {
  constructor(props) {
    super(props);
    this.evaluateInput = this.evaluateInput.bind(this);
  }

  evaluateInput(event) {
    const input_value = '' + this.refs.input_field.getValue();
    if (input_value.length > 0) {

      this.props.socketSend('debug-loop', input_value);
    }
  }

  render() {
    const { debug_info } = this.props;
    var result_info = null;
    if (debug_info && debug_info.trace) {
      result_info = <ResultInfo input={debug_info.input}
                                trace={debug_info.trace}
                                score={debug_info.score} />;
    }

    return (
      <main>
        <div className="row">
          <div className="col s4" style={{margin:'0', padding:'0'}}>
            <Tabs style={{margin:'0', padding:'0'}}>
              <Tab label="Test Input">
                <div className="">
                  <div className="row">
                    <div className="col s10">
                      <TextField ref="input_field" multiLine={true} floatingLabelText="Input"
                                 style={{width: '100%'}} />
                    </div>
                    <div className="col s2">
                      <FloatingActionButton mini={true} style={{marginTop:'30px'}}
                                            onClick={this.evaluateInput}>
                        <ContentAdd />
                      </FloatingActionButton>
                    </div>
                  </div>
                </div>
              </Tab>
            </Tabs>
          </div>
          <div className="col s8" style={{margin:'0', padding:'0'}}>
            <Tabs  style={{margin:'0', padding:'0'}}>
              <Tab label="Test Result">
                <div className="">
                  { result_info }
                </div>
              </Tab>
            </Tabs>
          </div>
        </div>
      </main>
    );
  }
}

DebugContainer.propTypes = {
  debug_info: PropTypes.object.isRequired,
  socketSend: PropTypes.func.isRequired,
  // Injected by React Router
  children: PropTypes.node
}

function mapStateToProps(state) {
  return {
    debug_info: state.debug_info,
  }
}

export default connect(mapStateToProps, {
  socketSend
})(DebugContainer);
