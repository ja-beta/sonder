#!/bin/bash

# Variables
SERVICE_NAME="quote_pipeline"
REGION="us-central1"
PROJECT_ID="sonder-2813"
SERVICE_URL="https://us-central1-sonder-2813.cloudfunctions.net/quote_pipeline"

# Start the pipeline
start() {
    echo "Starting quote pipeline..."

    # Create/update the scheduler job
    gcloud scheduler jobs create http ${SERVICE_NAME}-job \
        --schedule="*/5 * * * *" \
        --uri="$SERVICE_URL" \
        --http-method=POST \
        --location="$REGION" \
        2>/dev/null || \
    gcloud scheduler jobs resume ${SERVICE_NAME}-job --location="$REGION"

    echo "Quote pipeline started and scheduled to run every 5 minutes"
}

# Stop the pipeline
stop() {
    echo "Stopping quote pipeline..."

    # Delete the scheduler job
    gcloud scheduler jobs pause ${SERVICE_NAME}-job \
        --quiet \
        --location="$REGION" \
        2>/dev/null || echo "Scheduler job already stopped"

    echo "Quote pipeline stopped"
}

# Update the schedule
update() {
    if [ -z "$2" ]; then
        echo "Please specify interval in minutes (e.g., update 10)"
        exit 1
    fi

    echo "Updating schedule to run every $2 minutes..."

    # Update the scheduler job
    gcloud scheduler jobs update http ${SERVICE_NAME}-job \
        --schedule="*/$2 * * * *" \
        --location="$REGION"

    echo "Schedule updated to run every $2 minutes"
}

# Show status
status() {
    echo "Checking pipeline status..."
    
    # Check scheduler status
    gcloud scheduler jobs describe ${SERVICE_NAME}-job \
        --location="$REGION" \
        2>/dev/null || echo "No scheduler job found"
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