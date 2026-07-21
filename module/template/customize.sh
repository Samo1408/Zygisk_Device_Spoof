# shellcheck disable=SC2034
SKIPUNZIP=1

DEBUG=@DEBUG@
SONAME=@SONAME@
SUPPORTED_ABIS="@SUPPORTED_ABIS@"

if [ "$BOOTMODE" ] && [ "$KSU" ]; then
  ui_print "- Installing from KernelSU app"
elif [ "$BOOTMODE" ] && [ "$APATCH" ]; then
  ui_print "- Installing from APatch app"
elif [ "$BOOTMODE" ] && [ "$MAGISK_VER_CODE" ]; then
  ui_print "- Installing from Magisk app"
else
  ui_print "*********************************************************"
  ui_print "! Install from recovery is not supported"
  ui_print "! Please install from KernelSU or Magisk app"
  abort    "*********************************************************"
fi

VERSION=$(grep_prop version "${TMPDIR}/module.prop")
ui_print "- Installing $SONAME $VERSION"

# check architecture
support=false
for abi in $SUPPORTED_ABIS
do
  if [ "$ARCH" == "$abi" ]; then
    support=true
  fi
done
if [ "$support" == "false" ]; then
  abort "! Unsupported platform: $ARCH"
else
  ui_print "- Device platform: $ARCH"
fi

# Extract verify.sh
ui_print "- Extracting verify.sh"
unzip -o "$ZIPFILE" 'verify.sh' -d "$TMPDIR" >&2
if [ ! -f "$TMPDIR/verify.sh" ]; then
  ui_print "*********************************************************"
  ui_print "! Unable to extract verify.sh!"
  ui_print "! This zip may be corrupted, please try downloading again"
  abort    "*********************************************************"
fi
. "$TMPDIR/verify.sh"

# Extract core module files
extract "$ZIPFILE" 'customize.sh'  "$TMPDIR/.vunzip"
extract "$ZIPFILE" 'verify.sh'     "$TMPDIR/.vunzip"
extract "$ZIPFILE" 'sepolicy.rule' "$TMPDIR"

ui_print "- Extracting module files"
extract "$ZIPFILE" 'module.prop'     "$MODPATH"
extract "$ZIPFILE" 'post-fs-data.sh' "$MODPATH"
extract "$ZIPFILE" 'sepolicy.rule'   "$MODPATH"
extract "$ZIPFILE" 'README.md'       "$MODPATH"

# Setup webroot
ui_print "- Extracting webroot"
mkdir -p "$MODPATH/webroot"
extract "$ZIPFILE" 'webroot/index.html'   "$MODPATH"
extract "$ZIPFILE" 'webroot/action.sh'    "$MODPATH"
extract "$ZIPFILE" 'webroot/packages.sh'  "$MODPATH"

# Setup config
ui_print "- Setting up config"
mkdir -p "$MODPATH/config"
extract "$ZIPFILE" 'config/config.json' "$MODPATH"
if [ ! -f "$MODPATH/config/config.json" ]; then
  echo '{"apps":[]}' > "$MODPATH/config/config.json"
fi

# Setup proc directory
mkdir -p "$MODPATH/proc"

# Setup zygisk directory and extract native libs
mkdir -p "$MODPATH/zygisk"

HAS32BIT=false && ([ "$(getprop ro.product.cpu.abilist32)" ] || [ "$(getprop ro.system.product.cpu.abilist32)" ]) && HAS32BIT=true

if [ "$ARCH" = "x86" ] || [ "$ARCH" = "x64" ]; then
  if [ "$HAS32BIT" = true ]; then
    ui_print "- Extracting x86 libraries"
    extract "$ZIPFILE" "lib/x86/lib$SONAME.so" "$MODPATH/zygisk/" true
    mv "$MODPATH/zygisk/lib$SONAME.so" "$MODPATH/zygisk/x86.so"
  fi
  ui_print "- Extracting x64 libraries"
  extract "$ZIPFILE" "lib/x86_64/lib$SONAME.so" "$MODPATH/zygisk" true
  mv "$MODPATH/zygisk/lib$SONAME.so" "$MODPATH/zygisk/x86_64.so"
else
  if [ "$HAS32BIT" = true ]; then
    ui_print "- Extracting 32-bit libraries"
    extract "$ZIPFILE" "lib/armeabi-v7a/lib$SONAME.so" "$MODPATH/zygisk" true
    mv "$MODPATH/zygisk/lib$SONAME.so" "$MODPATH/zygisk/armeabi-v7a.so"
  fi
  ui_print "- Extracting arm64 libraries"
  extract "$ZIPFILE" "lib/arm64-v8a/lib$SONAME.so" "$MODPATH/zygisk" true
  mv "$MODPATH/zygisk/lib$SONAME.so" "$MODPATH/zygisk/arm64-v8a.so"
fi

# Set permissions
ui_print "- Setting permissions"
set_perm_recursive "$MODPATH" 0 0 0755 0644
chmod 755 "$MODPATH/webroot/action.sh" 2>/dev/null
chmod 755 "$MODPATH/webroot/packages.sh" 2>/dev/null
chmod 755 "$MODPATH/post-fs-data.sh" 2>/dev/null

ui_print ""
ui_print "Zygisk Device Spoof v2.0 installed!"
ui_print "WebUI available at: $MODPATH/webroot"
