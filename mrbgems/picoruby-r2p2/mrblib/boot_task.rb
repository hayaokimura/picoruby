if RUBY_ENGINE == "mruby/c"
  require "numeric-ext"
end
require "machine"   # hal_init(): タイマー・クロック設定（USB init は含まない）
require "watchdog"
Watchdog.disable

# FLASH マウント（XIP FLASH はUSB不要でアクセス可能）
begin
  lfs = Littlefs.new(:flash, label: "R2P2")
  VFS.mount(lfs, "/")
rescue => _e
end

# デフォルト: USB HID 有効
$r2p2_usb_hid = true

# ユーザーの boot.rb を読み込み（例: $r2p2_usb_hid = false）
begin
  load "/boot.rb" if File.file?("/boot.rb")
rescue => _e
end

# 設定に基づき USB を初期化（ここで tud_init が呼ばれる）
Machine.usb_init(hid: $r2p2_usb_hid)
