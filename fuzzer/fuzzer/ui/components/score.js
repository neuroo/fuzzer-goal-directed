import React, { Component, PropTypes } from 'react';
import {connect} from 'react-redux';

import _ from 'lodash';

import Chart from 'react-d3-core';
var LineChart = require('react-d3-basic').LineChart;


class ScoreChart extends React.Component {
  constructor(props) {
    super(props);
  }

  render() {
    const {scores} = this.props;

    var chartData = [];
    var first_elmt = this.props.scores[0]
                   || {min: {absolute: Number.POSITIVE_INFINITY, diff: Number.POSITIVE_INFINITY},
                       max: {absolute: 0, diff: 0}};
    var min_score_abs = first_elmt.min.absolute,
        max_score_abs = first_elmt.max.absolute;

    _.each(scores.scores, (elmt) => {
      chartData.push({
        index: elmt.index,
        score_min_abs: elmt.min.absolute,
        score_max_abs: elmt.max.absolute,
      });

      if (elmt.min.absolute < min_score_abs) { min_score_abs = elmt.min.absolute; }
      if (elmt.max.absolute > max_score_abs) { max_score_abs = elmt.max.absolute; }
    });

    var width = this.props.width,
        height = this.props.height,
        margins = {left: 50, right: 10, top: 10, bottom: 50},
        title = "Score Evolution",
        chartSeries = [
          {
            field: 'score_min_abs',
            name: 'Min Absolute',
            color: '#00bcd4'
          },
          {
            field: 'score_max_abs',
            name: 'Max Absolute',
            color: '#ff4081'
          }
        ],
        x = function(d) {
          return d.index;
        },
        yScale = 'linear';

    return (
      <div>
        <LineChart margins= {margins} title={title} data={chartData}
                   width={width} height={height}
                   chartSeries={chartSeries} x={x} yScale={yScale}/>
      </div>
    );
  }
}


ScoreChart.propTypes = {
  // Injected by React Redux
  scores: PropTypes.object.isRequired,
  // Injected by React Router
  children: PropTypes.node
}

function mapStateToProps(state) {
  return {
    scores: state.scores,
  }
}

export default connect(mapStateToProps)(ScoreChart);
