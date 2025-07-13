var cEnv={};
var nsTmpl = (
	function() {
		return {
			cache : {},
			tmpl : function (str, data, _index) {
				var strng =
							"var p=[];with(obj){p.push('"+
							str
							.replace(/[\r\t\n]/g," ")
							.replace(/^\s+</g,"<")
							.replace(/>\s+$/g,">")
							.replace(/>\s+</g,"><")
							.replace(/'(?=[^%]*%>)/g,"\t")
							.split("'").join("\\'")
							.split("\t").join("'")
							.replace(/<%=(.+?)%>/g, "',$1,'")
							.split("<%").join("');")
							.split("%>").join("p.push('")
							+ "');}return p.join('');"
				;
				var fn = !/\W/.test(str) ?
					this.cache[str] = this.cache[str] || this.tmpl(document.getElementsByClassName(str)[0].innerHTML) : new Function("obj","_index",strng);
				if (data) return fn(data,_index);
				return data ? fn(data,_index) : fn;
			},
			tmplr : function (stmpl,obj)
			{
				var s=''
				for (var i=0; i<(obj?obj.length:0); i++) s+=this.tmpl(stmpl,obj[i],i);
				return s;
			}
		}
	}
)();
function ajaxReq2(quiet, method, a)
{
	var url = '/api'; // DEBUG
	if (!quiet) msg("Connecting...",1);
	var req = new XMLHttpRequest();
	req.open(method, url+(method=='GET'?'?'+a[0]:''),true);
	if (method=='POST') req.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
	req.onreadystatechange = function() {
		if (req.readyState == 4 && a[1]) {
			if (req.status == 200) a[1](req, a[2]);
			else a[1](null, a[2]);
		}
	}
	req.send(method=='GET'?null:a[0]);
}
function ajaxGet()  { ajaxReq2.call(this,0,'GET',arguments); }
function ajaxGetq() { ajaxReq2.call(this,1,'GET',arguments); }
function ajaxPost() { ajaxReq2.call(this,0,'POST',arguments); }
function ajaxPostq(){ ajaxReq2.call(this,1,'POST',arguments); }
function $(e) { return document.getElementById(e); }
function $$(e,t) { return (t?t:document).querySelectorAll(e); }
function $X(e) {e.parentElement.removeChild(e);}
function $V(e) { return encodeURIComponent($(e).value); }
function $vV(e) { return '&'+e+'='+$V('id-'+e); }
function $cV(e) { return '&'+e+'='+($('id-'+e).checked?'1':'0'); }
function $top(e,tag) {
	for(; e&&e!=document&&e.nodeType==1; e=e.parentNode)
		if (e.tagName==tag.toUpperCase()) return e;
	return null;
}
function $i(e,v) { (typeof(e)=='object'?e:$(e)).innerHTML = v; }
function extend(obj,tmpl) {
	obj=obj||{};
	for (var prop in tmpl)
		if (tmpl.hasOwnProperty(prop) && !obj.hasOwnProperty(prop)) obj[prop] = tmpl[prop];
	return obj;
}
function err(s)
{
	$('err').innerHTML = s;
	$('msg').innerHTML = "";
	setTimeout( msg.bind(this,'',1), 4000 );
}
function msg(s, keep)
{
	$('err').innerHTML = "";
	$('msg').innerHTML = s?s:'Ready';
	if (!keep) setTimeout( msg.bind(this,'',1), 4000 );
}
function htmlize(str)
{
	return str.replace(/'/,'&#x27;').replace(/&/g,'&amp;').replace(/"/g,'&quot;').replace(/\</g,'&lt;').replace(/\>/g,'&gt;');
}

function pjson (req) {
	if (!req) {
		err("Connection error");
		return 0;
	}
	try {
		var data = eval('('+req.responseText+')');
	} catch(e) {
		err("Response format error: "+htmlize(req.responseText),1);
		return 0;
	}
	return data;
}
var wds = ['mon','tue','wed','thu','fri','sat','sun','mo','tu','we','th','fr','sa','su','пн','вт','ср','чт','пт','сб','вс','пон','втр','срд','чтв','птн','сбт','вск'];
function $PP(str) {
	var m = str.match(/^(\S{2,3})?\s?(\d{1,2})?:(\d{1,2})?:?(\d{1,2})?/);
	if (!m) return '';
	var wd = m[1]||0;
	if (wd) {
		var wd = wds.indexOf(wd.toLowerCase());
		if (wd==-1) return '';
		wd %= 7;
		wd++;
	}
	return wd+':'+(m[2]|0)+':'+(m[3]|0)+':'+(m[4]|0);
}
function $PR(str) {
	function zz(v) { return v<10?'0'+v:''+v; }
	if (!str) return '';
	var m = str.split(':');
	return ((m[0]|0)==0?'':wds[(m[0]|0)-1+14]+' ')+zz(m[1]|0)+':'+zz(m[2]|0)+':'+zz(m[3]|0);
}
