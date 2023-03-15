import React, { Component, PropTypes } from 'react';
import {connect} from 'react-redux';
import ReactDOM from 'react-dom';

import _ from 'lodash';

import IconButton from 'material-ui/lib/icon-button';
import ActionAspectRatio from 'material-ui/lib/svg-icons/action/aspect-ratio';
import CircularProgress from 'material-ui/lib/circular-progress';

import d3 from 'd3';
import { hexbin } from 'd3-hexbin';


function egcd(a, b) {
  if (a == 0)
    return b;
  while (b != 0) {
    if (a > b)
      a = a - b;
    else
      b = b - a;
  }
  return a;
}


function computeTileDimensions(width, height, num_elements) {
  const px = Math.ceil(Math.sqrt(num_elements * width / height));
  const py = Math.ceil(Math.sqrt(num_elements * height / width));

  var side_x, side_y;

  if(Math.floor(px * height / width) * px < num_elements)
		side_x = height / Math.ceil(px * height / width);
	else
		side_x = width / px;


	if(Math.floor(py * width / height) * py < num_elements)
		side_y = width / Math.ceil(width * py / height);
	else
		side_y = height / py;

  var side = Math.floor(Math.max(side_x, side_y));

  var num_width = Math.floor(width / side),
      num_height = Math.floor(num_elements / (num_width - 1));

  console.log("Dimensions", num_elements, side, num_width, num_height)

  return {
    side,
    num_width,
    num_height
  };
}


function getMousePos(canvas, evt) {
  var rect = canvas.getBoundingClientRect();
  return {
    x: evt.clientX - rect.left,
    y: evt.clientY - rect.top
  };
}


//
// Because of react, we don't draw directly from the component into the canvas.
// instead we'll trigger (with requestAnimationFrame) a new redraw that will
// draw the latest coverage info.
//
class CanvasElement extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      update_frame: 0,
      coverage_info: {}
    };

    this.colors = d3.scale.linear()
                 .domain([0.0, 0.2, 1.0])
                 .range(["#B2EDF4", "#f9b754", "#ff4081"])
                .interpolate(d3.interpolateHcl);
    this.num_elements = null;
    this.dimensions = null;
    this.previous_coverage = null;

    this.forceRedrawScene = this.forceRedrawScene.bind(this);
    this.drawScene = this.drawScene.bind(this);
    this.getSize = this.getSize.bind(this);
    this.drawTile = this.drawTile.bind(this);
    this.displayElementInfo = this.displayElementInfo.bind(this);
  }

  componentDidMount() {
    // this.forceRedrawScene();
  }

  componentDidUpdate() {
    this.forceRedrawScene();
  }

  forceRedrawScene() {
    const { width, height } = this.props;
    var coverage_info = this.props.coverage_info;

    var canvas = ReactDOM.findDOMNode(this.refs.canvas);
    var context = canvas.getContext("2d");

    if (!coverage_info && this.previous_coverage === null) {
      context.fillStyle = '#efefef';
      context.fillRect(0, 0, width, height);
      return;
    }

    if (!coverage_info) {
      coverage_info = _.clone(this.previous_coverage);
    }
    else {
      this.previous_coverage = _.clone(coverage_info);
    }

    var $this = this;

    canvas.addEventListener('click', function(evt) {
      evt.preventDefault();
      var mousePos = getMousePos(canvas, evt);
      $this.displayElementInfo(mousePos);
    });

    context.clearRect(0, 0, width, height);

    if (this.num_elements === null) {
      this.num_elements =  Object.keys(coverage_info.coverage).length;
    }
    if (this.dimensions === null) {
      this.dimensions = computeTileDimensions(width, height, this.num_elements);
    }
    this.drawScene(context, coverage_info);
  }

  drawScene(context, coverage_info) {
    const keys = Object.keys(coverage_info.coverage);
    keys.sort((a, b) => {
      return parseInt(a) - parseInt(b);
    });
    var counter = 0;

    for (var i=0; i<this.dimensions.num_height; i++) {
      for (var j=0; j<this.dimensions.num_width; j++) {
        if (counter >= this.num_elements) {
          return;
        }
        this.drawTile(context, j, i, this.dimensions.side,
                     coverage_info.coverage[keys[counter]], keys[counter])
        counter++;
      }
    }
  }

  drawTile(context, num_width, num_height, side_length, intensity, element_id) {
    var font_size = 12;
    context.font = "12px Roboto";

    const color = this.colors(intensity);
    const start_x = num_width * side_length,
          start_y = num_height * side_length;

    const text = "" + element_id + ": " + Number(100. * intensity).toFixed(2) + "%";
    var measure_text = context.measureText(text);
    var no_text = false;
    while (measure_text.width > side_length) {
      font_size--;
      if (font_size < 8) {
        no_text = true;
        break;
      }
      context.font = "" + font_size + "px Roboto";
      measure_text = context.measureText(text);
    }

    var text_x = start_x + (side_length - measure_text.width) / 2;

    context.fillStyle = color;
    context.strokeStyle = '#efefef';
    context.lineWidth   = 3;
    context.fillRect(start_x , start_y,
                     start_x + side_length, start_y + side_length);
    context.strokeRect(start_x , start_y,
                       start_x + side_length, start_y + side_length);

    if (!no_text) {
      context.fillStyle = "#111";
      context.fillText(text,
                       text_x,
                       start_y + (side_length - 20) / 2 );
    }
  }

  displayElementInfo(mousePosition) {
    console.log('click', mousePosition);
  }

  getSize() {
    return document.getElementById('CoverageChartId').getBoundingClientRect();
  }

  render() {
    return (
      <canvas ref='canvas' width={this.props.width} height={this.props.height}/>
    );
  }
}


//
// Exposed coverage component
//
class CoverageChart extends React.Component {
  constructor(props) {
    super(props);
    this.is_mounted = false;
    this.state = {
      update_frame: 0
    };
    this.tick = this.tick.bind(this);
  }

  componentWillMount() {
    window.addEventListener('resize', this.handleResize, true);
  }

  componentWillUnmount() {
    this.is_mounted = false;
  }

  componentDidMount() {
    this.is_mounted = true;
    requestAnimationFrame(this.tick);
  }

  tick() {
    const $this = this;
    setTimeout(() => {
      if ($this.is_mounted) {
        $this.setState({
          update_frame: $this.state.update_frame + 1,
          coverage_info: _.clone($this.props.coverage_info)
        });
      }
      requestAnimationFrame($this.tick);
    }, 1000); // 1FPS
  }

  render() {
    return (
      <div>
        <CanvasElement width={this.props.width} height={this.props.height}
                       update_frame={this.state.update_frame}
                       coverage_info={this.state.coverage_info} />
      </div>
    );
  }

}


CoverageChart.propTypes = {
  coverage_info: PropTypes.object.isRequired,
  // Injected by React Router
  children: PropTypes.node
}

CoverageChart.defaultProps = {
  width: 500,
  height: 400,
};


function mapStateToProps(state) {
  return {
    coverage_info: state.coverage_info,
  }
}

export default connect(mapStateToProps)(CoverageChart);
