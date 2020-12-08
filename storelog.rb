#!/usr/bin/env ruby

require 'fcntl'

INITRX = /starting as:/

IO.foreach($*.shift).with_object([false, []]) { |l, aa|
  l =~ INITRX and (aa[0]=true; aa[1].clear)
  aa[0] and aa[1] << l
}.then do |inited, la|
  inited or raise ArgumentError, "bad log format"
  la.first.strip.split(/\s+/, 5).then do |a|
    [File.basename(a[3]), a[4], a[0]].concat($*).compact.map do |s|
      s.gsub(/\//, "\\").gsub(/\s+/, ?-)
    end.join(?-).gsub(/-+/, ?-) + ".log"
  end.then do |s|
    puts "copying log to #{s.dump}"
    IO.sysopen(s, Fcntl::O_WRONLY|Fcntl::O_EXCL|Fcntl::O_CREAT).then { |fd| IO.for_fd fd }.then do |f|
      la.each { |ll| f << ll }
    end
  end
end
