cleanup_script="/tmp/.recovery_actions"

if [ -x $cleanup_script ]; then
    . $cleanup_script
    echo "Cleanup done. Removing the cleanup scripts..."
    rm $cleanup_script
else
    echo "No cleanup script available. Assuming shutdown was graceful..."
fi
