import React, { Component, PropTypes } from 'react';
import {connect} from 'react-redux';
import {bindActionCreators} from 'redux';

import { socketSend, selectCrashId, selectTestCaseId } from '../actions';

import Tabs from 'material-ui/lib/tabs/tabs';
import Tab from 'material-ui/lib/tabs/tab';
import List from 'material-ui/lib/lists/list';
import ListItem from 'material-ui/lib/lists/list-item';

import TestCaseContainer from '../components/testcase';

class CrashContainer extends Component {
  constructor(props) {
    super(props);
    this.handleChangeCrashId = this.handleChangeCrashId.bind(this);
    this.handleChangeTestCaseId = this.handleChangeTestCaseId.bind(this);
  }

  componentDidMount() {
    this.props.socketSend('crashers');
  }

  handleChangeCrashId(id) {
    console.log('handleChangeCrashId', id);
    this.props.selectCrashId(id);
  }

  handleChangeTestCaseId(id) {
    this.props.socketSend('crash', id);
    this.props.selectTestCaseId(id);
  }

  render() {
    const { crash_info, crash_selection } = this.props;
    var $this = this;
    var crash_id_list = [];
    if (crash_info.crash_ids) {
      var crash_ids = Object.keys(crash_info.crash_ids);
      _.forEach(crash_ids, (crash_id) => {
        crash_id_list.push(
          <ListItem key={crash_id} primaryText={crash_id}
                    onClick={() => $this.handleChangeCrashId(crash_id)} />
        );
      });
    }
    else {
      crash_id_list.push(
        <ListItem key={'no_crash_yet'} primaryText="No crash yet." />
      );
    }

    var testcase_id_list = [];
    if (crash_selection.selected_crash_id) {
      _.forEach(crash_info.crash_ids[crash_selection.selected_crash_id], (testcase_id) => {
        testcase_id_list.push(
          <ListItem key={testcase_id} primaryText={testcase_id}
                    onClick={() => $this.handleChangeTestCaseId(testcase_id)} />
        );
      });
    }

    var testcase_repr = null;
    if (crash_selection.selected_testcase_id
      && (crash_selection.selected_testcase_id in crash_info.crash)) {
      var crash_data = crash_info.crash[crash_selection.selected_testcase_id];
      testcase_repr = <TestCaseContainer data={crash_data} />
    }
    return (
      <main>
        <div className="row">
          <div className="col s4" style={{margin:'0', padding:'0'}}>
            <Tabs style={{margin:'0', padding:'0'}}>
              <Tab label="Crash IDs">
                <div className="container">
                  <List>
                    { crash_id_list }
                  </List>
                </div>
              </Tab>
            </Tabs>
          </div>
          <div className="col s1" style={{margin:'0', padding:'0'}}>
            <Tabs  style={{margin:'0', padding:'0'}}>
              <Tab label="Testcases">
                <div className="container">
                  { testcase_id_list }
                </div>
              </Tab>
            </Tabs>
          </div>
          <div className="col s7" style={{margin:'0', padding:'0'}}>
            <Tabs style={{margin:'0', padding:'0'}}>
              <Tab label="Info">
                <div className="container">
                  { testcase_repr }
                </div>
              </Tab>
            </Tabs>
          </div>
        </div>
      </main>
    );
  }
}

CrashContainer.propTypes = {
  // Injected by React Redux
  crash_selection: PropTypes.object.isRequired,
  crash_info: PropTypes.object.isRequired,

  socketSend: PropTypes.func.isRequired,
  selectCrashId: PropTypes.func.isRequired,
  selectTestCaseId: PropTypes.func.isRequired,

  // Injected by React Router
  children: PropTypes.node
}

function mapStateToProps(state) {
  return {
    crash_selection: state.crash_selection,
    crash_info: state.crash_info
  }
}

export default connect(mapStateToProps, {
  socketSend,
  selectCrashId,
  selectTestCaseId
})(CrashContainer);
