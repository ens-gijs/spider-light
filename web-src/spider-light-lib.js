"use strict";
let nextUid = 0;
const KEY_UP_ARROW = 38;
const KEY_DOWN_ARROW = 40;


// From https://gist.github.com/paulkaplan/5184275
// which is based on http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/
// Start with a temperature, in Kelvin, somewhere between 1000 and 40000.
function colorTemperatureToRGB(kelvin){
  let temp = kelvin / 100;
  let red, green, blue;
  if (temp <= 66) { 
    red = 255; 
    green = temp;
    green = 99.4708025861 * Math.log(green) - 161.1195681661;
    if( temp <= 19){
      blue = 0;
    } else {
      blue = temp - 10;
      blue = 138.5177312231 * Math.log(blue) - 305.0447927307;
    }
  } else {
    red = temp - 60;
    red = 329.698727446 * Math.pow(red, -0.1332047592);
    green = temp - 60;
    green = 288.1221695283 * Math.pow(green, -0.0755148492 );
    blue = 255;
  }
  red = clamp(Math.round(red), 0, 255);
  green = clamp(Math.round(green), 0, 255);
  blue = clamp(Math.round(blue), 0, 255);
  return {
    r : red,
    g : green,
    b : blue,
    hex: `${zeroPad(red.toString(16), 2)}${zeroPad(green.toString(16), 2)}${zeroPad(blue.toString(16), 2)}`
  }
}

function clamp(value, min, max) {
  if (value < min){ return min; }
  if (value > max){ return max; }
  return value;
}

function zeroPad(value, minLength) {
  let out;
  if (typeof value === 'number') {
    out = value.toString(10);
  } else {
    out = '' + value;
  }
  while (out.length < minLength) {
    out = '0' + out;
  }
  return out;
}

function newElement(type, attrs, style) {
  let $el = document.createElement(type);
  if (attrs) {
    Object.assign($el, attrs);
    if (attrs['class']) {
      if (!Array.isArray(attrs['class'])) {
        let classes = attrs['class'].split(' ');
        for (const c of classes) {
          $el.classList.add(c);
        }
      } else {
        for (const c of attrs['class']) {
          $el.classList.add(c);
        }
      }
    }
  }
  if (style) Object.assign($el.style, style);
  return $el;
}

/**
 * querySelector wrapper
 *
 * @param {string} selector Selector to query
 * @param {Element} [scope] Optional scope element for the selector
 */
function qs(selector, scope) {
	return (scope || document).querySelector(selector);
}

/**
 * addEventListener wrapper
 *
 * @param {Element|Window} target Target Element
 * @param {string} type Event name to bind to
 * @param {Function} callback Event callback
 * @param {boolean} [capture] Capture the event
 */
function $on(target, type, callback, capture) {
	target.addEventListener(type, callback, !!capture);
}

/**
 * Attach a handler to an event for all elements matching a selector.
 *
 * @param {Element} target Element which the event must bubble to
 * @param {string} selector Selector to match
 * @param {string} type Event name
 * @param {Function} handler Function called when the event bubbles to target
 *                           from an element matching selector
 * @param {boolean} [capture] Capture the event
 */
function $delegate(target, selector, type, handler, capture) {
	const dispatchEvent = event => {
		const targetElement = event.target;
		const potentialElements = target.querySelectorAll(selector);
		let i = potentialElements.length;

		while (i--) {
			if (potentialElements[i] === targetElement) {
				handler.call(targetElement, event);
				break;
			}
		}
	};

	$on(target, type, dispatchEvent, !!capture);
}

/**
 * Encode less-than and ampersand characters with entity codes to make user-
 * provided text safe to parse as HTML.
 *
 * @param {string} s String to escape
 *
 * @returns {string} String with unsafe characters escaped with entity codes
 */
const escapeForHTML = s => s.replace(/[&<]/g, c => c === '&' ? '&amp;' : '&lt;');

// https://stackoverflow.com/questions/1129216 (dynamicSort impl)
// People.sort(comparing("Name"));
// People.sort(comparing("-Name"));  // reverse order
// also works with arrays of arrays
// schedule.sort(comparing('-0'));  // reverse sort on element[0]
function comparing(property) {
    var sortOrder = 1;
    if(property[0] === "-") {
        sortOrder = -1;
        property = property.substr(1);
    }
    return function (a,b) {
        /* next line works with strings and numbers, 
         * and you may want to customize it to your needs
         */
        var result = (a[property] < b[property]) ? -1 : (a[property] > b[property]) ? 1 : 0;
        return result * sortOrder;
    }
}


class ScheduleEntry {
  constructor([time, colorTemp='', brightness=50]=[]) {
    this._uid = nextUid++;
    this.time = zeroPad(time / 60 |0, 2) + ':' + zeroPad(time % 60, 2);
    this.colorTemp = colorTemp;
    this.brightness = brightness;
  }
  
  toJson() {
    let hhmm = this.time.split(':');
    return [parseInt(hhmm[0], 10) * 60 + parseInt(hhmm[1], 10), parseInt(this.colorTemp, 10) || 0, parseInt(this.brightness, 10) || 0];
  }
}

class Schedule {
  constructor(container, rawSchedule) {
    this.entries = rawSchedule.map((s) => new ScheduleEntry(s)).sort(comparing('time'));
    this.$schedule = newElement('table', {class: 'schedule'});
    this.$schedule.innerHTML = '<thead><tr><td></td><td>Time</td><td>Color Temp</td><td>Brightness</td></tr></thead>'
        + '<tbody></tbody><tfoot><tr><td><button class="add-time">add</button></td><td><input type="time" class="new-time"></td></tr></tfoot>';
    this.$tbody = qs('tbody', this.$schedule);
    this.$newTime = qs('tfoot .new-time', this.$schedule);
    qs('tfoot .add-time', this.$schedule).onclick = () => this.onAddTimeAction();
    if (container) container.append(this.$schedule);
    
    $delegate(this.$schedule, '.remove', 'click', ({target}) => {
      let uid = target.parentNode.parentNode.dataset.uid;
			this.onRemoveItemAction(uid);
		});
    
    $delegate(this.$tbody, '.time', 'change', ({target}) => {
      let se = this.getScheduleEntryByElement(target);
      if (se.time != target.value) {
        this.getScheduleEntryByElement(target).time = target.value;
        this.render();
        const elem = qs(`[data-uid="${se._uid}"]`, this.$tbody);
        elem.classList.add('moved');
        setTimeout(() => elem.classList.remove('moved'), 500);
      }
		});
    
    $delegate(this.$tbody, '.color-temp', 'change', ({target}) => {
      this.getScheduleEntryByElement(target).colorTemp = target.value;
		});
    
    $delegate(this.$tbody, '.color-temp', 'input', ({target}) => {
      let value = clamp(parseInt(target.value, 10), 2700, 6500);
      if (value == target.value) {
        let rgb = colorTemperatureToRGB(target.value);
        target.nextSibling.style.backgroundColor = '#' + rgb.hex;
      } else {
        target.nextSibling.style.backgroundColor = 'red';
      }
		});
    
//    $delegate(this.$tbody, '.color-temp', 'keydown', ({target, keyCode}) => {
//      if (keyCode == KEY_UP_ARROW) {
//        target.value = clamp(parseInt(target.value, 10) + 100, 2700, 6500);
//        updateSwatch.call(this);
//      } else if (keyCode == KEY_DOWN_ARROW) {
//        target.value = clamp(parseInt(target.value, 10) - 100, 2700, 6500);
//        updateSwatch.call(this);
//      }
//		});
    
    $delegate(this.$tbody, '.brightness', 'change', ({target}) => {
      this.getScheduleEntryByElement(target).brightness = target.value;
		});
    
    this.render();
  }
  
  getScheduleEntryByUid(uid) {
    for (let e of schedule.entries) {
      if (e._uid == uid) return e;
    }
    return null;
  }
  
  getScheduleEntryByElement(el) {
    return this.getScheduleEntryByUid(el.parentNode.parentNode.dataset.uid);
  }
  
  onRemoveItemAction(uid) {
		const elem = qs(`[data-uid="${uid}"]`, this.$tbody);
		if (elem) {
      this.entries = this.entries.filter(e => e._uid != uid);
      elem.classList.add('deleting');
      setTimeout(() => this.$tbody.removeChild(elem), 200);
		}
  }
  
  onAddTimeAction() {
    let time = this.$newTime.value;
    if (time) {
      console.log("ADD NEW", time);
      let se = this._newEntry(time);
      if (se) {
        this.$newTime.value = null;
        let row = qs(`[data-uid="${se._uid}"]`, this.$tbody);
        row.classList.add('newly-added');
        qs('.color-temp', row).focus();
        setTimeout(() => row.classList.remove('newly-added'), 500);
      }
    }
  }
  
  _newEntry(time) {
    if (!this.entries.some(e => e.time === time)) {
      let se = new ScheduleEntry();
      se.time = time;
      console.log("NEW ScheduleEntry", time, se);
      this.entries.push(se);
      this.render();
      return se;
    }
    return null;
  }
  
	render() {
		this.$tbody.innerHTML = this.entries.sort(comparing('time')).reduce((a, entry) => a + `
      <tr data-uid="${entry._uid}">
        <td><button class="remove">X</button></td>
        <td><input class="time" type="time" value="${entry.time}"></td>
        <td><input class="color-temp" type="text" size=4  value="${entry.colorTemp}"><canvas width=16 height=16 class="swatch" style="background-color: #${colorTemperatureToRGB(entry.colorTemp).hex}"></canvas></td>
        <td><input class="brightness" type="range" min=0 max=100 value=${entry.brightness}></td>
      </tr>`, '');
	}
  
  toJson() {
    return this.entries.sort(comparing('time')).map((s) => s.toJson());
  }
}


class ChannelConfig{
  constructor(container, channelId, {'enabled': enabled, 'max_brightness': maxBrightness, 'swap_warm_cool_sig': swapWarmCool}) {
    this.$dom = newElement('fieldset', {class: 'channel-config'});
    this.$dom.innerHTML = `
      <legend>Channel <span>${channelId}</span></legend>
      <div><label><input type="checkbox" name="enabled"> Enabled</label></div>
      <div><label><input type="checkbox" name="swapWarmCool"> Swap Warm & Cool Signals</label></div>
      <div><label>Max Brightness <input type="range" name="brightness" min=0 max=100 oninput="this.nextElementSibling.value = this.value"> <output></output></label></div>`;
    this.$channelId = qs('legend span', this.$dom);
    this.$enabled = qs('input[name="enabled"]', this.$dom);
    this.$swapWarmCool = qs('input[name="swapWarmCool"]', this.$dom);
    this.$brightness = qs('input[name="brightness"]', this.$dom);
    this.$brightnessOutput = qs('input[name="brightness"] + output', this.$dom);
    this.enabled = enabled;
    this.brightness = maxBrightness;
    this.swapWarmCool = swapWarmCool;
    if (container) container.append(this.$dom);
  }

  set channelId(value) {this.$channelId.innerText = value}
  get channelId() {return this.$channelId.innerText}

  set enabled(value) {this.$enabled.checked = value}
  get enabled() {return !!this.$enabled.checked}

  set swapWarmCool(value) {this.$swapWarmCool.checked = value}
  get swapWarmCool() {return !!this.$swapWarmCool.checked}

  set brightness(value) {
    value = clamp(value |0, 0, 100);
    this.$brightness.value = value;
    this.$brightnessOutput.value = value; 
  }
  get brightness() {return  parseInt(this.$brightness.value, 10)}
  
  toJson() {
    return {
      'enabled': this.enabled,
      'swap_warm_cool_sig': this.swapWarmCool,
      'max_brightness': this.brightness
    }
  }
}


class SpiderLightConfig {
  constructor(container) {
    this.$dom = container;
    this.channelA = null;
    this.channelB = null;
    this.schedule = null;
  }
  
  async _getConfig() {
    return fetch('query/config.json')
      .then((response) => {
        if (!response.ok) {
          throw new Error('Network response was not OK');
        }
        return response.json();
      });
  }
  
  reload() {
    this.$dom.innerHTML = '<div class="loading">loading...</div>';
    this.channelA = null;
    this.channelB = null;
    this.schedule = null;
    this._getConfig().then((data) => {
      this.$dom.innerHTML = '';
      this.channelA = new ChannelConfig(this.$dom, 'A', data.channel_a);
      this.channelB = new ChannelConfig(this.$dom, 'B', data.channel_b);
      this.schedule = new Schedule(this.$dom, data.schedule);
    });
  }
  
  toJson() {
    return {
      time_zone: "EST5EDT",
      max_lum_at_color_temp: 5800,
      channel_a: this.channelA.toJson(),
      channel_b: this.channelB.toJson(),
      schedule: this.schedule.toJson()
    };
  }
}



































