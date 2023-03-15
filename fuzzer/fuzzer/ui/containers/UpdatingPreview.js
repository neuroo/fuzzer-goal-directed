import React, { Component, PropTypes } from 'react';
import {connect} from 'react-redux';

import moment from 'moment';
import _ from 'lodash';

import { Sparklines, SparklinesLine,
         SparklinesSpots, SparklinesReferenceLine } from 'react-sparklines';

import FlatButton from 'material-ui/lib/flat-button';
import ToolbarGroup from 'material-ui/lib/toolbar/toolbar-group';


class UpdatingPreviewContainer extends Component {
  constructor(props) {
    super(props);
  }

  render() {
    const { elapsed_time, generated_testcases, rates } = this.props.data;

    var pretty_time = (!!elapsed_time)
                    ? (moment('2000-01-01 00:00:00').add(
                        moment.duration({seconds: elapsed_time})
                      ).format('HH:mm:ss'))
                    : '00:00:00';

    var rate = generated_testcases / elapsed_time;
    if (!isNaN(rate)) {
      rate = rate.toFixed(1);
    }
    else {
      rate = '...';
    }

    var copied_rates = _.clone(rates);

    return (
      <ToolbarGroup firstChild={false} float="right">
        <FlatButton disabled={true}>Elapsed: {pretty_time} • Rate: {rate}/s</FlatButton>
        <Sparklines data={copied_rates} limit={25} height={50} width={150}>
          <SparklinesLine style={{ fill: "none" }} />
          <SparklinesSpots />
          <SparklinesReferenceLine style={{ stroke: 'grey', strokeOpacity: 0.75, strokeDasharray: '2, 2' }} />
        </Sparklines>
      </ToolbarGroup>
    );
  }
}

UpdatingPreviewContainer.propTypes = {
  // Injected by React Redux
  data: PropTypes.object.isRequired,
  // Injected by React Router
  children: PropTypes.node
}

function mapStateToProps(state) {
  return {
    previewData: state.previewData,
  }
}

export default connect(mapStateToProps)(UpdatingPreviewContainer);
