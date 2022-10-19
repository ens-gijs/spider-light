#ifndef _SPIDER_LIGHT_CSS_H
#define _SPIDER_LIGHT_CSS_H
const char SPIDER_LIGHT_CSS[] PROGMEM = R"====(
body {
  font-family: Tahoma, Verdana, sans-serif;
  display: flex;
  justify-content: center;
}
fieldset{
  margin: 1em auto;
  background-color: #f9f9f9;
  box-shadow: 0 0 20px rgb(0 0 0 / 15%);
  border: none;
}
legend{
  border-radius: 0.5em;
  padding: 0.1em 1em;
  background-color: #f9f9f9;
  border: 1px solid #00000012;
  box-shadow: 0 0 10px rgb(0 0 0 / 15%);
  margin-left: -1em;
}
table {
  border-collapse: collapse;
  margin: 25px 0;
  font-size: 0.9em;
  min-width: 400px;
  box-shadow: 0 0 20px rgba(0, 0, 0, 0.15);
}
table thead tr {
  background-color: #009879;
  color: #ffffff;
  text-align: left;
}
table th,
table td {
  padding: 12px 15px;
}
table tbody tr {
  border-bottom: 1px solid #dddddd;
}
table tbody tr:nth-of-type(even) {
  background-color: #f3f3f3;
}
table tbody tr:last-of-type {
  border-bottom: 2px solid #009879;
}

.swatch {
  outline: 1px solid black;
  vertical-align: text-bottom;
  margin-left: 0.5em;
}

.deleting {
  background-color: rgba(255, 0, 0, .5) !important;
}

.moved, .newly-added {
  background-color: rgba(128, 192, 192, .5) !important;
}
#config{
  display: inline-table;
}
.config-controls{
  padding: 0.5em;
  background-color: #f9f9f9;
  box-shadow: 0 0 20px rgba(0, 0, 0, 0.15);
}
.config-controls.error{
  background-color: rgba(255, 0, 0, .5) !important;
}
.config-controls.ok{
  background-color: rgba(128, 192, 192, .5) !important;
}
.reload{
}
.upload{
  float: right;
}
.general-settings input{
  width: 100%;
  margin-bottom: .5em;
}
.brightness + output{
  width: 2em;
  display: inline-block;
  text-align: right;
}
)====";
#endif