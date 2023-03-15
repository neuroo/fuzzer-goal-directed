
function delay_connection(socket, callback){
  setTimeout(() => {
    if (socket.readyState === 1) {
      if(callback != null) {
        callback(socket);
      }
    } else {
      delay_connection(socket, callback);
    }
  }, 5);
}


class Socket {
  constructor() {
    console.log('construct Socket');
    this.ws_address = window.ws_address;
    this.ws = new WebSocket(this.ws_address);
    this.ws.onconnect = (event) => {
      console.log('connected');
    }
  }

  send(kind, data=null) {
    console.log('Socket::send-', JSON.stringify({
      kind: kind,
      data: data
    }));
    if (typeof kind !== 'string') {
      console.error('format of socket message');
      throw 'wrong format!!!';
    }

    const $this = this;
    delay_connection(this.ws,
                    (ws) => ws.send(JSON.stringify({
                      kind: kind,
                      data: data
                    })));
  }
};

const socket = new Socket();
export default socket;
