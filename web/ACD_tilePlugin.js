/*

For multiple external scripts.

"external_scripts": [
    "/HA_tilePlugin.js",
    "/ACD_tilePlugin.js"
],

for one.

"external_script": "/ACD_tilePlugin.js",
  "ACD_tilePlugin": {
    "ACD_server": "http://aqualinkd-cm:88",
    "ACD_entity_ids": [
      "AquachemD",
      "CS_3",
      "TEMP_1",
      "PH_1",
      "ORP_1",
      "PMP_1",
      "PMP_2"
    ]
  },

*/

let _acd_socket_di;
let _acd_devices = {};

acd_setupTiles();


function acd_setupTiles() {

  // If aqualinkd has not added tiles, wait.
  if (document.getElementById("Filter_Pump") === null) {
    setTimeout(acd_setupTiles, 100);
    return;
  }

  if (_config?.ACD_tilePlugin && _config.ACD_tilePlugin.ACD_server) {
    acd_start_websocket(_config.ACD_tilePlugin.ACD_server);
  } else {
    /* Not configured to run */
    console.log("ACD_tilePlugin not configured, skipping.");
  }
}

function acd_tile_click(id) {
  console.log("Clicked tile with id: " + id);
}

function formatDeviceValue(val) {
    // 1. Determine precision: 1 decimal if < 100, else 0
    let precision = (val < 100) ? 1 : 0;
    
    // 2. Format to string, then convert back to Number to strip ".0"
    // 3. Convert back to String for your function
    return String(Number(val.toFixed(precision)));
}

function acd_update_devices(data) {
  var deviceArray = Object.values(data.devices);
  deviceArray.forEach(device => {
    if (_config.ACD_tilePlugin.ACD_entity_ids.includes(device.id)) {
      device.status = device.status.toLowerCase();
      device.state = device.status.toLowerCase();
      device.name = device.label;
      device.orig_type = device.type;
      if (device.value) {
        device.orig_value = device.value;
        device.value = formatDeviceValue(device.value);
      }
      let uom = null;

      switch (device.type) {
        case "sensor":
          if (device.id.startsWith("TEMP")) {
            device.type = "temperature";
            uom = '&deg;';
          } else if (device.id.startsWith("ORP")) {
            device.type = "value";
            uom = 'mV';
          } else {
            device.type = "value";
          }
          break;
        case "binary_sensor":
          device.type = "switch";
          break;
      }

      if (document.getElementById(device.id) == null) {
        //console.log("Device " + device.id + " not found, creating tile.");
        // Add id to display array.
        if (device?.id && !_devices.includes(device.id)) {
          _devices.push(device.id);
        }
        //console.log(device);
        createTile(device);
        
        // Make sure we use out own callback for button press.
        let tile = document.getElementById(device.id);
        if (tile) {
          tile.setAttribute('onclick', "acd_tile_click('" + device.id + "')");
          if (uom) {
            console.log("Setting UOM for "+device.id+" to "+uom);
            tile.setAttribute('UOM', uom);  
          }
        } else {
          console.log("Error: Could not find tile for device " + device.id + " after creating it.");
        }

        
      }

      //console.log("Device " + device.id + " found, updating tile.");

      switch (device.orig_type) {
        case "switch":
          //setTileOn(device.id, ((device.state == 'off') ? 'off' : 'on'), null);
          setTileOn(device.id, device.status.toLowerCase());
          setTileOnText(device.id, formatSatus(device.status));
          break;
        case "sensor":
          setTileOn(device.id, ((device.state == 'off') ? 'off' : 'on'), null);
          setTileValue(device.id, device.value);
          break;
        case "binary_sensor":
          var el = document.getElementById(device.id + '_tile_icon_value');
            if (el && device.state == 'on') { // overwrite standard SVG icon
              el.style.backgroundImage = _sensorOnUri;
            } else if (el && device.state == 'off') {
              el.style.backgroundImage = _sensorOffUri;
            }
          break;
      }

      //setTileOn(device.id, ((device.state == 'off') ? 'off' : 'on'), null);
    }
  });
}

function acd_get_devices() {
  var msg = {
    uri: "devices"
  };
  _acd_socket_di.send(JSON.stringify(msg));
}

function acd_start_websocket(server) {
  _acd_socket_di = new WebSocket(server);
  try {
    _acd_socket_di.onopen = function () {
      // success!
      acd_get_devices();
      // Set recurring fetch every 1 minute
      if (!window.devicesInterval) {
        window.devicesInterval = setInterval(() => { acd_get_devices(); }, 60 * 1000);
      }
    }
    _acd_socket_di.onmessage = function got_packet(msg) {
      document.getElementById("header").classList.remove("error");
      var data = JSON.parse(msg.data);
      if (data.type == 'devices') {
        _acd_devices = data;
        acd_update_devices(_acd_devices);
      }
    }
    _acd_socket_di.onclose = function () {
      // something went wrong
      console.log("WebSocket Closed:");
      console.log("Code:   " + event.code);    // Numeric code (e.g., 1000, 1006)
      console.log("Reason: " + event.reason);  // Text explanation from server
      console.log("Clean:  " + event.wasClean); // Boolean: did the TCP handshake close properly?

      setElementHTML("message", '  Connection error!  ');
      document.getElementById("header").classList.add("error");
      // Try to reconnect every 5 seconds.
      setTimeout(function () {
        startWebsockets()
      }, 5000);
    }
  } catch (exception) {
    alert('<p>Error' + exception);
  }
}

/*
   UI related stuff
*/

function createSensorIcon(color, largeSize) {
  var sensorContent;
  if (!largeSize) {
    /* Standard Size: Centered probe with side pulse lines */
    sensorContent = `<svg xmlns="http://www.w3.org/2000/svg"
                          viewBox="0 0 45 45"
                          stroke="`+ color + `" fill="none"
                          stroke-width="3.5" stroke-linecap="round">
                          <path d="M22.5 10 V30" />
                          <circle cx="22.5" cy="30" fill="`+ color + `" r="3" />
                          <line x1="8" y1="20" x2="16" y2="20" />
                          <line x1="29" y1="20" x2="37" y2="20" />
                        </svg>`;
  } else {
    /* Large Size: Expanded probe body and wider pulse lines */
    sensorContent = `<svg xmlns="http://www.w3.org/2000/svg"
                      viewBox="0 0 45 45"
                      stroke="`+ color + `" fill="none"
                      stroke-width="4.5" stroke-linecap="round">
                      <path d="M22.5 7 V28" />
                      <circle cx="22.5" cy="32" fill="`+ color + `" r="4" />
                      <line x1="5" y1="18" x2="15" y2="18" />
                      <line x1="30" y1="18" x2="40" y2="18" />
                    </svg>`;
  }
  const encodedSensorContent = encodeURIComponent(sensorContent);
  return `url("data:image/svg+xml;charset=utf-8,${encodedSensorContent}")`;
}

if (_config?.["colors" + _theme]?.tile_on_background) {
  _sensorOnUri = createSensorIcon(_config["colors" + _theme].tile_on_background, false);
} else {
  _sensorOnUri = createSensorIcon("rgb(255, 255, 255)", false);
}

if (_config?.["colors" + _theme]?.tile_icon_background_color_disabled) {
  _sensorOffUri = createSensorIcon(_config["colors" + _theme].tile_icon_background_color_disabled, false);
} else {
  _sensorOffUri = createSensorIcon("rgb(110, 110, 110)", false);
}
