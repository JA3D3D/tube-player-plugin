// Stand-in for YouTube's IFrame API used by the page tests.
(function () {
  var titles = { dQw4w9WgXcQ: 'How to make a tearout growl in Operator', '9bZkp7q19f0': 'Riddim drums in 10 minutes' };
  window.YT = { PlayerState: { UNSTARTED: -1, ENDED: 0, PLAYING: 1, PAUSED: 2, BUFFERING: 3, CUED: 5 } };
  YT.Player = function (el, o) {
    var s = this; window.__yt = s; s.o = o; s.vid = o.videoId || ''; s.t = (o.playerVars && o.playerVars.start) || 0; s.state = -1; s.rate = 1; s.muted = false; s.calls = []; s.dur = 212;
    var host = document.getElementById(el), f = document.createElement('iframe');
    f.id = el; f.srcdoc = '<body style="margin:0;height:100vh;display:flex;align-items:center;justify-content:center;background:linear-gradient(135deg,#2b3a55,#5a2b4a);color:#fff;font:600 18px sans-serif">fake video</body>';
    host.parentNode.replaceChild(f, host);
    setTimeout(function () { o.events.onReady({ target: s }); }, 60);
    setInterval(function () { if (s.state === 1) { s.t += 0.1 * s.rate; if (s.t >= s.dur) { s.t = s.dur; s.set(0); } } }, 100);
  };
  var P = YT.Player.prototype;
  P.set = function (x) { this.state = x; this.o.events.onStateChange({ data: x, target: this }); };
  P.playVideo = function () { this.calls.push('play'); if (window.__blockAutoplay && !this.muted && !navigator.userActivation.hasBeenActive) return; this.set(1); };
  P.pauseVideo = function () { this.calls.push('pause'); this.set(2); };
  P.seekTo = function (t) { this.calls.push('seek ' + Math.round(t * 10) / 10); this.t = t; };
  P.getCurrentTime = function () { return this.t; };
  P.getDuration = function () { return this.vid ? this.dur : 0; };
  P.getPlayerState = function () { return this.state; };
  P.setPlaybackRate = function (r) { this.rate = r; this.calls.push('rate ' + r); this.o.events.onPlaybackRateChange && this.o.events.onPlaybackRateChange({ data: r }); };
  P.getPlaybackRate = function () { return this.rate; };
  P.getVideoData = function () { return { title: titles[this.vid] || '', video_id: this.vid }; };
  P.loadVideoById = function (a) { this.calls.push('load ' + a.videoId + ' ' + a.startSeconds); this.vid = a.videoId; this.t = a.startSeconds || 0; this.playVideo(); };
  P.cueVideoById = function (a) { this.vid = a.videoId; this.t = a.startSeconds || 0; this.set(5); };
  P.loadPlaylist = function () { this.set(1); };
  P.mute = function () { this.muted = true; this.calls.push('mute'); };
  P.unMute = function () { this.muted = false; this.calls.push('unmute'); };
  P.setVolume = function () {};
  window.__ytError = function (c) { window.__yt.o.events.onError({ data: c }); };
  setTimeout(function () { window.onYouTubeIframeAPIReady && window.onYouTubeIframeAPIReady(); }, 30);
})();
