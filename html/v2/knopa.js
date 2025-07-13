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
          this.cache[str] = this.cache[str] || this.tmpl(document.getElementsByClassName(str)[0].innerHTML) : new Function("obj",strng);
        if (data) return fn(data);
        return data ? fn(data) : fn;
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
function ajaxRequest(quiet, method, url, params, callback, extra)
{
  url += 'i.php'; // DEBUG!
  if (!quiet) msg("Connecting...",1);
//  params = params+'&rand='+Math.random();
  var ajax = new XMLHttpRequest();
  ajax.open(method, url+'?'+params);
  ajax.onreadystatechange = function() {
    if (ajax.readyState == 4 && callback) {
      if (ajax.status == 200) callback(ajax, extra);
      else callback(null, extra);
    }
  }
  ajax.send(null);
}
function ajaxGet(url, params, callback, extra)   { ajaxRequest(0, 'get', url, params, callback, extra); }
function ajaxGetq(url, params, callback, extra)  { ajaxRequest(1, 'get', url, params, callback, extra); }
//function ajaxPost(url, params, callback, extra)  { ajaxRequest(0, 'post', url, params, callback, extra); }
function $(e) { return document.getElementById(e); }
function $$(e,t) { return (t?t:document).querySelectorAll(e); }
function $V(e) { return encodeURIComponent($(e).value); }
function $v(e) { return $(e).value; }
function $nV(e) { return '&'+e+'='+$V('id-'+e); }
function $cV(e) { return '&'+e+'='+($('id-'+e).checked?'1':'0'); }
function $top(e,tag) {
  for(; e&&e!=document&&e.nodeType==1; e=e.parentNode)
    if (e.tagName==tag.toUpperCase()) return e;
  return null;
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
function pjson (req) {
  if (!req) {
    err("Connection error");
    return 0;
  }
  try {
    var data = eval('('+req.responseText+')');
  } catch(e) {
    err("Response format error");
    return 0;
  }
  return data;
}
