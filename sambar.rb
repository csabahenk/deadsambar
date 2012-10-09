#!/usr/bin/env ruby

# For terms of usage see LICENSE

def hms t
  h = t.to_i/3600
  t -= h * 3600
  m = t.to_i/60
  t -= m*60
  [h, m, t.to_i]
end

def gm s
  g = s.to_i >> 10
  s -= g << 10
  [g, s.to_i]
end

pbw = Integer($*[0]||100)

s = STDIN.read(8)
fsize = s.unpack("q")[0].to_f
raise "negative size" if fsize < 0
t0 = Time.now


pb = [" "] * pbw

while d = STDIN.read(8)
  t = Time.now - t0
  x = "="
  d = d.unpack("q")[0]
  if d < 0
    d *= -1
    x = "*"
  end
  pbi = d/fsize * pbw
  pb[pbi] = x unless pb[pbi] == "*"
  dm = d / (1 << 20).to_f
  h,m,s = hms(t)
  gb,mb = gm(dm)
  STDERR << "\r%02d:%02d:%02d %3dG %3dM %.1fM/s [%s]   " % [h, m, s, gb, mb, dm/t, pb.join]
end
STDERR.puts
  
