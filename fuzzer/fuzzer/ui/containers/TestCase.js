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

import TextLines from '../components/hex';


class SimpleTestCaseContainer extends Component {
  render() {
    const { testcase } = this.props;
    return (
      <div className="row">
        <Paper zDepth={1} style={{ padding: '5px', width: '100%', marginTop: '10px',
                                   fontFamily:'monospace', fontSize: '11px', border: '0'}}>
          Position: {testcase.position} <br />
          Length: {testcase.length}<br />
          Goal: {testcase.score.goal.norm}, Coverage: {testcase.score.edge.norm}<br />
          <br />
          <TextLines lines={testcase.repr} />
        </Paper>
      </div>
    );
  }
}

SimpleTestCaseContainer.propTypes = {
  testcase: PropTypes.object.isRequired,

  // Injected by React Router
  children: PropTypes.node
}



class TestCaseContainer extends Component {
  render() {
    const { stats_info } = this.props;

    var best_testcases = [];
    var statistic_rows = [];

    if (stats_info.stats) {
      _.forEach(stats_info.stats, (stats_value, stats_name) => {
        var stats_value_adg = Number(stats_value).toFixed(2);
        statistic_rows.push(
          <tr key={stats_name}>
            <td>{stats_name}</td>
            <td>{stats_value_adg}</td>
          </tr>
        );
      });
    }

    if (stats_info.best) {
      _.forEach(stats_info.best, (best_testcase) => {
        best_testcases.push(<SimpleTestCaseContainer key={best_testcase.position}
                                                     testcase={best_testcase} />);
      });
    }

    return (
      <main>
        <div className="row">
          <div className="col s4" style={{margin:'0', padding:'0'}}>
            <Tabs style={{margin:'0', padding:'0'}}>
              <Tab label="Statistics">
                <div className="container">
                <table>
                  <thead>
                    <tr>
                        <th>Measure</th>
                        <th>Value</th>
                    </tr>
                  </thead>
                  <tbody>
                    { statistic_rows }
                  </tbody>
                </table>
                </div>
              </Tab>
            </Tabs>
          </div>
          <div className="col s8" style={{margin:'0', padding:'0'}}>
            <Tabs  style={{margin:'0', padding:'0'}}>
              <Tab label="Current Best Testcases">
                <div className="container">
                  { best_testcases }
                </div>
              </Tab>
            </Tabs>
          </div>
        </div>
      </main>
    );
  }
}

TestCaseContainer.propTypes = {
  stats_info: PropTypes.object.isRequired,

  socketSend: PropTypes.func.isRequired,
  // Injected by React Router
  children: PropTypes.node
}


function mapStateToProps(state) {
  return {
    stats_info: state.stats_info,
  }
}

export default connect(mapStateToProps, {
  socketSend
})(TestCaseContainer);
