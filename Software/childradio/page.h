
#include "my_common.h"

const char PAGE_NEW[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="de">
	<head>
		<title>Kinderradio</title>
		<style>
			.fanrpmslider {
				width: 30%;
				height: 55px;
				outline: none;
			}
			.bodytext {
				font-family: "Verdana", "Arial", sans-serif;
				font-size: 24px;
				text-align: left;
				font-weight: lighter;
				border-radius: 5px;
				display:inline;
			}
			.bodyhead {
				font-family: "Verdana", "Arial", sans-serif;
				font-size: 24px;
				text-align: right;
				font-weight: bolder;
				border-radius: 5px;
				display:inline;
			}
			.tableth {
				font-family: "Verdana", "Arial", sans-serif;
				font-size: 24px;
				text-align: left;
				font-weight: bold;
			}
			.tabletd {
				font-family: "Verdana", "Arial", sans-serif;
				font-size: 24px;
				text-align: left;
				font-weight: normal;
			}
      .nw {display:inline;}
		</style>
    <script>
      function txt(doc, tag) {
        const el = doc.getElementsByTagName(tag)[0];
        return el && el.textContent ? el.textContent : "";
      }

      function initData(){
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState === 4 && this.status === 200) {
            const xml = this.responseXML;
            document.getElementById("balance").value   = txt(xml, 'balance');
            document.getElementById("tone_low").value  = txt(xml, 'tone_low');
            document.getElementById("tone_band").value = txt(xml, 'tone_band');
            document.getElementById("tone_high").value = txt(xml, 'tone_high');
            document.getElementById("bat0").innerHTML  = txt(xml, 'bat0');
            document.getElementById("bat1").innerHTML  = txt(xml, 'bat1');
            document.getElementById("time1").innerHTML = txt(xml, 'time1');
            document.getElementById("compV").innerHTML = txt(xml, 'compV');
            document.getElementById("compD").innerHTML = txt(xml, 'compD');
            document.getElementById("hostnametext").innerHTML = txt(xml, 'hostname');

            // Channel-Status (Buttontext + URL-Feld)
            const act = parseInt(txt(xml, 'act')) || 0;
            if (act > 0) {
              document.getElementById("CHbutton").innerHTML = "Leeren";
              document.getElementById("url").value = txt(xml, 'url');
            } else {
              document.getElementById("CHbutton").innerHTML = "Setzen";
              document.getElementById("url").value = "";
            }
          }
        };
        xhttp.open("GET", "/ajax_init?ts=" + Date.now(), true);
        xhttp.send();
      }


      function getData() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState === 4) {
            if (this.status === 200) {
              const xml = this.responseXML;
              // Strings sicher ziehen (auch wenn Tag leer ist)
              document.getElementById("Station_Title").innerHTML = txt(xml, 'station_title');
              document.getElementById("streaminfo").innerHTML    = txt(xml, 'streaminfo');
              document.getElementById("streamtitle").innerHTML   = txt(xml, 'streamtitle');
              document.getElementById("bitrate").innerHTML       = txt(xml, 'bitrate');
              document.getElementById("icyurl").innerHTML        = txt(xml, 'icyurl');
              document.getElementById("lasthost").innerHTML      = txt(xml, 'lasthost');
              document.getElementById("wr_wl").innerHTML         = txt(xml, 'wr_wl');
              document.getElementById("time2").innerHTML         = txt(xml, 'time2');
              document.getElementById("bat_level").innerHTML     = txt(xml, 'bat_level');
              document.getElementById("bat_voltage").innerHTML   = txt(xml, 'bat_voltage');
              document.getElementById("heap").innerHTML          = txt(xml, 'heap');

              // Inputs setzen
              document.getElementById("vol").value               = txt(xml, 'vol');
              document.getElementById("alarm_time").value        = txt(xml, 'alarm_time');
              document.getElementById("alarmVol").value          = txt(xml, 'alarmVol');
              document.getElementById("rgb_brightness").value    = txt(xml, 'rgb_brightness');
              document.getElementById("rgb_time").value          = txt(xml, 'rgb_time');
              document.getElementById("rgb_pattern").selectedIndex = parseInt(txt(xml, 'rgb_pattern')) || 0;

              document.getElementById("hostnametext").innerHTML  = txt(xml, 'hostname');

              // Alarm-Flags sicher parsen
              const x = parseInt(txt(xml, 'alarm_byte')) || 0;
              document.getElementById("alarm_act").checked = (x & 128) > 0;
              document.getElementById("alarm_mo").checked  = (x & 1)   > 0;
              document.getElementById("alarm_tu").checked  = (x & 2)   > 0;
              document.getElementById("alarm_we").checked  = (x & 4)   > 0;
              document.getElementById("alarm_th").checked  = (x & 8)   > 0;
              document.getElementById("alarm_fr").checked  = (x & 16)  > 0;
              document.getElementById("alarm_sa").checked  = (x & 32)  > 0;
              document.getElementById("alarm_so").checked  = (x & 64)  > 0;
            } else {
              console.error('ajax_refresh HTTP', this.status);
            }
          }
        };
        // Cache-Busting + führenden Slash nutzen
        xhttp.open("GET", "/ajax_refresh?ts=" + Date.now(), true);
        xhttp.send();
      }
      setInterval(function(){ getData(); }, 500);
      window.addEventListener('DOMContentLoaded', getData);

      function sendSlider(slider, value){
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/" + slider + "?value=" + value, true);
        xhr.onreadystatechange = function(){
          if (xhr.readyState === 4) setTimeout(getData, 50);
        };
        xhr.send();
      }

      function sendAlarmSlider(name, value){
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/alarm?" + name + "=" + value, true);
        xhr.onreadystatechange = function(){
          if (xhr.readyState === 4) setTimeout(getData, 50);
        };
        xhr.send();
      }

      function handlehostname(){
        var value = document.getElementById("newhostname").value;
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/hostname?hostname=" + encodeURIComponent(value), true);
        xhr.onreadystatechange = function(){
          if (xhr.readyState === 4) setTimeout(getData, 100);
        };
        xhr.send();
      }

      function handleurl(exe){
        var channel = document.getElementById("channel").value;
        var urlStr  = document.getElementById("url").value;
        var setornotset = -1;

        if (exe === "true") {
          const label = document.getElementById("CHbutton").innerHTML;
          if (label === "Setzen") setornotset = 1;
          if (label === "Leeren") setornotset = 0;
        }

        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState === 4 && this.status === 200) {
            const xml = this.responseXML;
            const act = parseInt(txt(xml, "act")) || 0;
            if (act > 0) {
              document.getElementById("CHbutton").innerHTML = "Leeren";
              document.getElementById("url").value = txt(xml, "url");
            } else {
              document.getElementById("CHbutton").innerHTML = "Setzen";
              document.getElementById("url").value = "";
            }
            // gleich die Live-Daten nachziehen
            setTimeout(getData, 50);
          }
        };
        // URL und Channel sicher codieren
        const qs = "/channel?channel=" + encodeURIComponent(channel) +
                  "&url=" + encodeURIComponent(urlStr) +
                  "&setornotset=" + encodeURIComponent(setornotset) +
                  "&ts=" + Date.now();
        xhttp.open("GET", qs, true);
        xhttp.send();
      }

      </script>
	</head>
	<body onload="initData()">
		<header>
		</header>
		<main>
			<div class="bodyhead"> Kinderradio </div>
			<br>
      <br>

        <table>
          <tr>
						<th class="tableth">Cannel: </th>
						<td class="tabletd">
              <select id="channel" onchange="handleurl('false')">
                <option value="0">0</option>
                <option value="1">1</option>
                <option value="2">2</option>
                <option value="3">3</option>
                <option value="4">4</option>
                <option value="5">5</option>
                <option value="6">6</option>
                <option value="7">7</option>
              </select> 
            </td>
            <td>
              <button id="CHbutton" onclick="handleurl('true')">Clear</button>
            </td>
					</tr>
          <tr>
            <td class="tabletd" colspan="3">
              <input type="text" id="url" size="URL_LENGTH">
            </td>
          </tr>
        </table>
      <!-- <img src="/image" alt="Cover" style="width:200px;height:200px;object-fit:cover;border-radius:8px;"> --!>
				<table>
					<tr>
						<th class="tableth">Station: </th>
						<td class="tabletd"><p class="nw" id="Station_Title">Not requested...</p></td>
					</tr>
					<tr>
						<th class="tableth">Streaminfo: </th>
						<td class="tabletd"><p class="nw" id="streaminfo">Not requested...</p></td>
					</tr>
					<tr>
						<th class="tableth">Streamtitle: </th>
						<td class="tabletd"><p class="nw" id="streamtitle">Not requested...</p></td>
					</tr>
					<tr>
						<th class="tableth">Bitrate: </th>
						<td class="tabletd"><p class="nw" id="bitrate">Not requested...</p></td>
					</tr>
					<tr>
						<th class="tableth">Icyurl: </th>
						<td class="tabletd"><p class="nw" id="icyurl">Not requested...</p></td>
					</tr>
					<tr>
						<th class="tableth">Lasthost: </th>
						<td class="tabletd"><p class="nw" id="lasthost">Not requested...</p></td>
					</tr>
				</table>
        <br>
        <table>
          <tr>
            <th class="tableth">Alarm:</th>
            <td class="tabledt"> <input type="datetime-local" id="alarm_time" onchange="sendAlarmSlider(this.id, this.value)">   </td>
          </tr>
          <tr>
            <th class="tableth">Aktivieren:</th>
            <td class="tabledt"> <input type="checkbox" id="alarm_act" onchange="sendAlarmSlider(this.id, this.checked)">   </td>
          </tr>
          <tr>
            <th class="tableth">Alarm Volumen:</th>
            <td class="tabledt"><input type="range" id="alarmVol" min="0" max="21" value="0" step="1" onchange="sendAlarmSlider(this.id, this.value)"></td>
          </tr>
          <tr>
            <th class="tableth">Montag:</th>
            <td class="tabledt"> <input type="checkbox" id="alarm_mo" onchange="sendAlarmSlider(this.id, this.checked)">   </td>
          </tr>
          <tr>
            <th class="tableth">Dienstag:</th>
            <td class="tabledt"> <input type="checkbox" id="alarm_tu" onchange="sendAlarmSlider(this.id, this.checked)">   </td>
          </tr>
          <tr>
            <th class="tableth">Mittwoch:</th>
            <td class="tabledt"> <input type="checkbox" id="alarm_we" onchange="sendAlarmSlider(this.id, this.checked)">   </td>
          </tr>
          <tr>
            <th class="tableth">Donnerstag:</th>
            <td class="tabledt"> <input type="checkbox" id="alarm_th" onchange="sendAlarmSlider(this.id, this.checked)">   </td>
          </tr>
          <tr>
            <th class="tableth">Freitag:</th>
            <td class="tabledt"> <input type="checkbox" id="alarm_fr" onchange="sendAlarmSlider(this.id, this.checked)">   </td>
          </tr>
          <tr>
            <th class="tableth">Samstag:</th>
            <td class="tabledt"> <input type="checkbox" id="alarm_sa" onchange="sendAlarmSlider(this.id, this.checked)">   </td>
          </tr>
          <tr>
            <th class="tableth">Sonntag:</th>
            <td class="tabledt"> <input type="checkbox" id="alarm_so" onchange="sendAlarmSlider(this.id, this.checked)">   </td>
          </tr>

        </table>

			<br>
      <h3>Equalizer:</h3>
      <table>
        <tr>
          <th class="tableth">Volumen:</th>
          <td class="tabledt"><input type="range" id="vol" min="0" max="21" value="0" step="1" onchange="sendSlider(this.id, this.value)"></td>
        </tr>
        <tr>
          <th class="tableth">Balance:</th>
          <td class="tabledt"><input type="range" id="balance" min="-16" max="16" value="0" step="1" onchange="sendSlider(this.id, this.value)"></td>
        </tr>
        <tr>
          <th class="tableth">Hochpass:</th>
          <td class="tabledt"><input type="range" id="tone_high" min="-40" max="6" value="0" step="1" onchange="sendSlider(this.id, this.value)"></td>
        </tr>
        <tr>
          <th class="tableth">Bandpass:</th>
          <td class="tabledt"><input type="range" id="tone_band" min="-40" max="6" value="0" step="1" onchange="sendSlider(this.id, this.value)"></td>
        </tr>
        <tr>
          <th class="tableth">Tiefpass:</th>
          <td class="tabledt"><input type="range" id="tone_low" min="-40" max="6" value="0" step="1" onchange="sendSlider(this.id, this.value)"></td>
        </tr>
      </table>
      <br>
      <h3>System Information:</h3>
      <table>
        <tr>
          <th class="tableth">Webradio CPU Auslastung:</th>
          <td class="tabletd"><p class="nw" id="wr_wl">Not requested...</p>%</td>
        </tr>
        <tr>
          <th class="tableth">Totaler Heap:</th>
          <td class="tabletd"><p class="nw" id="heap">Not requested...</p>Bytes</td>
        </tr>
        <tr>
          <th class="tableth">Batterie beim booten:</th>
          <td class="tabletd"><p class="nw" id="bat0">Not requested...</p>%</td>
        </tr>
        <tr>
          <th class="tableth">Batterie beim Power down:</th>
          <td class="tabletd"><p class="nw" id="bat1">Not requested...</p>%</td>
        </tr>
        <tr>
          <th class="tableth">Sleep-Time:</th>
          <td class="tabletd"><p class="nw" id="time1">Not requested...</p> sek</td>
        </tr>
        <tr>
          <th class="tableth">Operation-Time:</th>
          <td class="tabletd"><p class="nw" id="time2">Not requested...</p> sek</td>
        </tr>
        <tr>
          <th class="tableth">Batterie Spannung:</th>
          <td class="tabletd"><p class="nw" id="bat_voltage">Not requested...</p>mV</td>
        </tr>
         <tr>
          <th class="tableth">Batterie Level:</th>
          <td class="tabletd"><p class="nw" id="bat_level">Not requested...</p>%</td>
        </tr>
        <tr>
          <th class="tableth">Compiler Version</th>
          <td class="tabletd"><p class="nw" id="compV">Not requested...</p></td>
        </tr>
        <tr>
          <th class="tableth">Compiler Date</th>
          <td class="tabletd"><p class="nw" id="compD">Not requested...</p></td>
        </tr>
      </table>

      <br>
      <h3>Beleuchtung:</h3>
      <table>
        <tr>
          <th class="tableth">Muster:</th>
          <td class="tabledt">
             <select id="rgb_pattern" onchange="sendSlider(this.id, this.value)">
              <option value="0">ColorPalette        </option>
              <option value="1">Fire2012WithPalette </option>
              <option value="2">Fire                </option>
              <option value="3">Pacifica            </option>
              <option value="4">Cyclon              </option>
            </select>            
          </td>
        </tr>
        <tr>
          <th class="tableth">Leuchtkraft:</th>
          <td class="tabledt"><input type="range" id="rgb_brightness" min="1" max="255" value="32" step="8" onchange="sendSlider(this.id, this.value)"></td>
        </tr>
        <tr>
          <th class="tableth">Geschwindlichkeit:</th>
          <td class="tabledt"><input type="range" id="rgb_time" min="1" max="255" value="32" step="8" onchange="sendSlider(this.id, this.value)"></td>
        </tr>
      </table>


    <br>
    <h3>Log:</h3> 
      <a href="log">Download Logfile</a>;
    <br>
    <h3>Update:</h3>
    <form method="POST" action="/update" enctype="multipart/form-data">    
      <input type="file" name="update">
      <input type="submit" value="Update">
    </form>

    <br>

    <table>
      <tr>
        <th class="tableth">Hostname: </th>
        <td class="tabletd"><p class="nw" id="hostnametext">Not requested...</p></td>
      </tr>
    </table>
    <input type="text" id="newhostname" size="HOSTNAME_LENGTH"> <button onclick="handlehostname('true')">Setzen</button>

        

		</main>
		<footer>
			
		</footer>
	</body>
</html>
)=====";
