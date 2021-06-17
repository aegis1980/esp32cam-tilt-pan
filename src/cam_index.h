

const PROGMEM char INDEX_HTML[] = R"rawliteral(
<html>
  <head>
    <title>ESP32-CAM Robot</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body,html {
        margin: 0;
        padding:0;
        height:100%; 
        width:100%;
        font-family: Arial, Helvetica, sans-serif;
        text-align: center
      }
      img {  
        width: auto ;
        max-width: 100% ;
        height: auto ; 
      }
      table { margin-left: auto; margin-right: auto; }
      td { padding: 8 px; }
      #control {
          position: absolute;
          top: 0;
          left: 0;
          margin: 20;
          padding: 5;
          background-color: rgba(255, 255, 255, 0.5);
      }
      .button {
        background-color: red;
        border: none;
        color: white;
        padding: 10px 20px;
        text-align: center;
        text-decoration: none;
        display: inline-block;
        font-size: 18px;
        margin: 6px 3px;
        cursor: pointer;
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        -khtml-user-select: none;
        -moz-user-select: none;
        -ms-user-select: none;
        user-select: none;
        -webkit-tap-highlight-color: rgba(0,0,0,0);
      }

    </style>
  </head>
  <body>
    <img src="" id="photo" >
    <div id="control">
      <table>
        <tr><td colspan="3" align="center"><button class="button" onmousedown="onButtonClick('up');" ontouchstart="onButtonClick('up');">Up</button></td></tr>
        <tr><td align="center"><button class="button" onmousedown="onButtonClick('left');" ontouchstart="onButtonClick('left');">Left</button></td><td align="center"></td><td align="center"><button class="button" onmousedown="onButtonClick('right');" ontouchstart="onButtonClick('right');">Right</button></td></tr>
        <tr><td colspan="3" align="center"><button class="button" onmousedown="onButtonClick('down');" ontouchstart="onButtonClick('down');">Down</button></td></tr>                   
      </table>
      <table>
        <tr><td align="left"><strong>Tilt</strong></td><td id="val-tilt" align="right">0</td></tr>        
        <tr><td align="left"><strong>Pan</strong></td><td id="val-pan" align="right">0</td></tr>                
      </table>
    </div>
   <script>

   function onButtonClick(x) {
        var xhr = new XMLHttpRequest();

        xhr.onload = function(e) {
            if (xhr.status == 200) {
                console.log(xhr.response)
                const myjson = JSON.parse(xhr.responseText); // JSON response  
                document.getElementById("val-tilt").innerHTML = myjson.tilt
                document.getElementById("val-pan").innerHTML = myjson.pan
            }
        };

        xhr.open("GET", "/action?go=" + x, true);
        xhr.send();
   }
   window.onload = document.getElementById("photo").src = window.location.href.slice(0, -1) + ":81/stream";
  </script>
  </body>
</html>
)rawliteral";