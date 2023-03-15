import React, { Component, PropTypes } from 'react';
import {connect} from 'react-redux';

import _ from 'lodash';

import TextField from 'material-ui/lib/text-field';
import RaisedButton from 'material-ui/lib/raised-button';
import FlatButton from 'material-ui/lib/flat-button';

import Table from 'material-ui/lib/table/table';
import TableHeaderColumn from 'material-ui/lib/table/table-header-column';
import TableRow from 'material-ui/lib/table/table-row';
import TableHeader from 'material-ui/lib/table/table-header';
import TableRowColumn from 'material-ui/lib/table/table-row-column';
import TableBody from 'material-ui/lib/table/table-body';

import ScoreChart from '../components/score';
import CoverageChart from '../components/coverage';

import { navigate, updateRouterState } from '../actions';


class ProgressContainer extends Component {
  constructor(props) {
    super(props);
    this.handleChange = this.handleChange.bind(this);
  }

  handleChange(nextValue) {
    this.props.navigate(`/${nextValue}`);
  }

  render() {
    const { global_info, target_info } = this.props;

    var option_rows = [];
    _.forEach(global_info.options, (option_value, option_name) => {
      var option_value_str = '' + option_value;
      option_rows.push(
        <TableRow key={option_name}>
          <TableRowColumn>{option_name}</TableRowColumn>
          <TableRowColumn>{option_value_str}</TableRowColumn>
        </TableRow>
      );
    });

    return (
      <div className="">
        <div className="row">
          <div className="col s6">
            <div className="section">
              <h5>Coverage</h5>
                <div className="valign-wrapper" id="CoverageChartId">
                  <div className="valign center-align" style={{margin:"0 auto"}}>
                    <CoverageChart width={500} height={400} />
                  </div>
                </div>
            </div>
          </div>
            <div className="col s6">
              <div className="section">
                <h5>Best Testcases Scores</h5>
                <div className="valign-wrapper">
                  <div className="valign center-align" style={{margin:"auto"}}>
                    <ScoreChart width={500} height={400} />
                  </div>
                </div>
              </div>
            </div>
        </div>
        <div className="divider"></div>
        <div className="row">
          <div className="col s6">
            <div className="section">
              <h5>Target Summary</h5>
                <Table selectable={false}>
                  <TableBody displayRowCheckbox={false}>
                    <TableRow key={'target_info.num_functions'}>
                      <TableRowColumn>Number of functions</TableRowColumn>
                      <TableRowColumn>
                        <RaisedButton label={target_info.num_functions}
                                      secondary={true}/>
                      </TableRowColumn>
                    </TableRow>
                    <TableRow key={'target_info.num_blocks'}>
                      <TableRowColumn>Number of blocks</TableRowColumn>
                        <TableRowColumn>
                          <RaisedButton label={target_info.num_blocks}
                                        secondary={true}/>
                        </TableRowColumn>
                    </TableRow>
                    <TableRow key={'target_info.num_goals'}>
                      <TableRowColumn>Number of derived goals</TableRowColumn>
                        <TableRowColumn>
                          <RaisedButton label={target_info.num_goals}
                                        secondary={true}/>
                        </TableRowColumn>
                    </TableRow>
                  </TableBody>
                </Table>
            </div>
          </div>
          <div className="col s6">
            <div className="section">
              <h5>Options</h5>
                <TextField multiLine={false} floatingLabelText="Command" value={global_info.command} style={{width: '100%'}}/><br/>
                <TextField multiLine={false} floatingLabelText="Intermediate Directory" value={global_info.idir} style={{width: '100%'}} /><br/>
                <Table selectable={false}>
                  <TableHeader displaySelectAll={false} adjustForCheckbox={false}>
                    <TableRow>
                      <TableHeaderColumn>Option Name</TableHeaderColumn>
                      <TableHeaderColumn>Option Value</TableHeaderColumn>
                    </TableRow>
                  </TableHeader>
                  <TableBody displayRowCheckbox={false}>
                    {option_rows}
                  </TableBody>
                </Table>
            </div>
          </div>
        </div>
      </div>
    );
  }
}

ProgressContainer.propTypes = {
  // Injected by React Redux
  scores: PropTypes.object.isRequired,
  global_info: PropTypes.object.isRequired,
  target_info: PropTypes.object.isRequired,
  coverage_info: PropTypes.object.isRequired,

  navigate: PropTypes.func.isRequired,
  updateRouterState: PropTypes.func.isRequired,

  // Injected by React Router
  children: PropTypes.node
}

function mapStateToProps(state) {
  return {
    scores: state.scores,
    global_info: state.global_info,
    target_info: state.target_info,
    coverage_info: state.coverage_info
  }
}

export default connect(mapStateToProps, {
  navigate,
  updateRouterState,
})(ProgressContainer);
