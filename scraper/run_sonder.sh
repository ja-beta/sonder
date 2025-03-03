#!/bin/bash

# Variables
SERVICE_NAME="quote_pipeline"
SCHEDULER_NAME="quote_pipeline_schedule"  # Updated to match your actual scheduler name
REGION="us-central1"
PROJECT_ID="sonder-2813"
SERVICE_URL="https://us-central1-sonder-2813.cloudfunctions.net/quote_pipeline"

# Start the pipeline
start() {
    echo "Starting quote pipeline..."

    # Create/update the scheduler job
    gcloud scheduler jobs create http ${SCHEDULER_NAME} \
        --schedule="*/5 * * * *" \
        --uri="$SERVICE_URL" \
        --http-method=POST \
        --location="$REGION" \
        2>/dev/null || \
    gcloud scheduler jobs resume ${SCHEDULER_NAME} --location="$REGION"

    echo "Quote pipeline started and scheduled to run every 5 minutes"
}

# Stop the pipeline
stop() {
    echo "Stopping quote pipeline..."

    # Pause the scheduler job
    gcloud scheduler jobs pause ${SCHEDULER_NAME} \
        --quiet \
        --location="$REGION" \
        2>/dev/null || echo "Scheduler job not found"
    
    echo "Quote pipeline paused - job '${SCHEDULER_NAME}' has been disabled"
}

# Update the schedule
update() {
    if [ -z "$2" ]; then
        echo "Please specify interval in minutes (e.g., update 10)"
        exit 1
    fi

    echo "Updating schedule to run every $2 minutes..."

    # Update the scheduler job
    gcloud scheduler jobs update http ${SCHEDULER_NAME} \
        --schedule="*/$2 * * * *" \
        --location="$REGION"

    echo "Schedule updated to run every $2 minutes"
}

# Show status
status() {
    echo "Checking pipeline status..."
    
    # Check scheduler status
    echo "Scheduler status:"
    gcloud scheduler jobs describe ${SCHEDULER_NAME} \
        --location="$REGION" \
        2>/dev/null || echo "No scheduler job found"

    # Check if scheduler is enabled or disabled
    echo -e "\nAll scheduler jobs for this function:"
    gcloud scheduler jobs list --location="$REGION" | grep ${SERVICE_NAME} || echo "No scheduler jobs found"

    # Check cloud functions status
    echo -e "\nCloud Functions status:"
    gcloud functions list --regions="$REGION" || echo "No functions found"
    
    # Check for active executions
    echo -e "\nRecent executions (last 5):"
    gcloud functions logs read ${SERVICE_NAME} \
        --region="$REGION" \
        --limit=5 \
        --sort-by=~time
}

# Main script logic
case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    update)
        update "$@"
        ;;
    status)
        status
        ;;
    *)
        echo "Usage: $0 {start|stop|update <minutes>|status}"
        exit 1
        ;;
esac