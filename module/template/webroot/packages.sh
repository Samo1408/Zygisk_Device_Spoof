#!/system/bin/sh
# KernelSU WebUI - Packages API
# Returns list of installed packages as JSON

# Get package list
if [ "$1" = "all" ]; then
    # All packages including system
    PACKAGES=$(pm list packages 2>/dev/null | sed 's/package://g')
else
    # Third-party only (exclude system)
    PACKAGES=$(pm list packages -3 2>/dev/null | sed 's/package://g')
fi

# Output as JSON array
echo -n '['
FIRST=true
for pkg in $PACKAGES; do
    # Get app label
    LABEL=$(pm resolve-activity --brief $pkg 2>/dev/null | head -1)
    [ -z "$LABEL" ] && LABEL="$pkg"
    
    if $FIRST; then
        FIRST=false
    else
        echo -n ','
    fi
    echo -n "\"$pkg\""
done
echo ']'
