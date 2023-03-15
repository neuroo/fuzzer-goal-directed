import React, { Component, PropTypes } from 'react';
import {connect} from 'react-redux';
import ReactDOM from 'react-dom';

import _ from 'lodash';

import TextField from 'material-ui/lib/text-field';
import Paper from 'material-ui/lib/paper';

import TextLines from './hex';

class HexRepr extends React.Component {
  render() {
    var { repr } = this.props;
    return (
      <Paper zDepth={1} style={{ padding: '5px', width: '100%',
                                fontFamily:'monospace', fontSize: '11px', border: '0' }}>
        <TextLines lines={repr} />
      </Paper>
    );
  }
}

const INTERESTING_REGISTERS = [
  "sp", "esp", "rsp",
  "bp", "ebp", "rbp",
  "ip", "eip", "rip",
];


class TraceElement extends React.Component {
  render() {
    const { elmt } = this.props;
    var registers = [];
    _.forEach(elmt.registers, (reg_value, reg_key) => {
      if (INTERESTING_REGISTERS.indexOf(reg_key.toLowerCase()) >= 0) {
        registers.push(<span style={{color:'#ff4081'}}>{reg_key}: {reg_value}<br /></span>)
      }
      else {
        registers.push(<span>{reg_key}: {reg_value}<br /></span>)
      }
    });

    return (
      <div className="row">
        <div className="col s6">
          <Paper zDepth={1} style={{ padding: '5px', width: '100%',
                                    fontFamily:'monospace', fontSize: '11px', border: '0' }}>
            <div className="chip">frame #{ elmt.frame }</div><br />

            Function: <span style={{color:'#ff4081'}}>{elmt.function_name}</span><br/>
            Function Base: {elmt.function_base}<br/>
            File Base: {elmt.file_base_address}<br/>
            Source File: {elmt.source_file_name}<br />
            Source Line: {elmt.source_line_base}<br />
          </Paper>
        </div>
        <div className="col s6">
          <Paper zDepth={1} style={{ padding: '5px', width: '100%',
                                    fontFamily:'monospace', fontSize: '11px', border: '0' }}>
            { registers }
          </Paper>
        </div>
      </div>
    );
  }
}


class TraceRepr extends React.Component {
  render() {
    var { trace } = this.props;
    var repr_trace = [];

    _.forEach(trace, (trace_elmt) => {
      repr_trace.push(<TraceElement key={trace_elmt.frame} elmt={trace_elmt} />);
    });

    return (
      <div>
        { repr_trace }
      </div>
    );
  }
}



class TestCaseContainer extends React.Component {
  constructor(props) {
    super(props);
  }

  render() {
    const { data } = this.props;
    console.log('TestCaseContainer', data);



    return (
      <div>
        <h5>Testcase ({data.testcase.length} bytes)</h5>
        <HexRepr key={data.testcase.buffer} repr={data.testcase.repr} />

        <h5>Info</h5>
        <Paper zDepth={1} style={{ padding: '5px', width: '100%',
                                  fontFamily:'monospace', fontSize: '11px', border: '0' }}>
          Reason:  {data.info.crash_info.reason}<br />
          Address: {data.info.crash_info.address}<br />
          Thread:  {data.info.crash_info.thread}<br />
          Exploitability:  {data.info.crash_info.exploitability}
        </Paper>

        <h5>Trace</h5>
        <TraceRepr trace={data.info.trace} />

      </div>
    );
  }
}


TestCaseContainer.propTypes = {
  crash_info: PropTypes.object.isRequired,
  // Injected by React Router
  children: PropTypes.node
}

function mapStateToProps(state) {
  return {
    crash_info: state.crash_info,
  }
}

export default connect(mapStateToProps)(TestCaseContainer);
