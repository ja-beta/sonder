#!/bin/bash

# Variables
SERVICE_NAME="quote_pipeline"
SCHEDULER_NAME="quote_pipeline_schedule"  
REGION="us-central1"
PROJECT_ID="sonder-2813"
SERVICE_URL="https://us-central1-sonder-2813.cloudfunctions.net/quote_pipeline"

start() {
    echo "Starting quote pipeline..."
    
    # Get collection parameter (default to quotes_v0 if not specified)
    #remember to also set the collection in the config.py files!!!!!
    COLLECTION=${2:-"quotes_v7"}
    echo "Using collection: $COLLECTION"
    
    # Delete the existing job (clean slate approach)
    gcloud scheduler jobs delete ${SCHEDULER_NAME} \
        --quiet \
        --location="$REGION" \
        --project="${PROJECT_ID}" \
        2>/dev/null || echo "No job to delete"
    
    # Create a fresh job with correct schedule and collection parameter
    gcloud scheduler jobs create http ${SCHEDULER_NAME} \
        --schedule="*/20 * * * *" \
        --uri="${SERVICE_URL}?collection=${COLLECTION}" \
        --http-method=POST \
        --location="$REGION" \
        --project="${PROJECT_ID}"

    echo "Quote pipeline started and scheduled to run every 20 minutes with collection: ${COLLECTION}"
}

stop() {
    echo "Stopping quote pipeline..."

    # Pause the scheduler job
    gcloud scheduler jobs pause ${SCHEDULER_NAME} \
        --quiet \
        --location="$REGION" \
        --project="${PROJECT_ID}" \
        2>/dev/null || echo "Scheduler job not found"
    
    # Additionally make sure no executions happen by updating the schedule to a far future time
    # December 31st at midnight - runs once a year at most
    gcloud scheduler jobs update http ${SCHEDULER_NAME} \
        --schedule="0 0 31 12 *" \
        --location="$REGION" \
        --project="${PROJECT_ID}" \
        2>/dev/null || echo "Could not update scheduler"
        
    echo "Quote pipeline completely stopped - job '${SCHEDULER_NAME}' has been disabled and rescheduled"
}

disable() {
    echo "Completely disabling quote pipeline..."

    # Delete the scheduler job
    gcloud scheduler jobs delete ${SCHEDULER_NAME} \
        --quiet \
        --location="$REGION" \
        --project="${PROJECT_ID}" \
        2>/dev/null || echo "Scheduler job not found or already deleted"
    
    echo "Quote pipeline disabled - scheduler job has been completely removed"
}

update() {
    if [ -z "$2" ]; then
        echo "Please specify interval in minutes (e.g., update 10)"
        exit 1
    fi

    echo "Updating schedule to run every $2 minutes..."

    # Update the scheduler job
    gcloud scheduler jobs update http ${SCHEDULER_NAME} \
        --schedule="*/$2 * * * *" \
        --location="$REGION" \
        --project="${PROJECT_ID}"

    echo "Schedule updated to run every $2 minutes"
}

status() {
    echo "Checking pipeline status..."
    
    # Check scheduler status
    echo "Scheduler status:"
    gcloud scheduler jobs describe ${SCHEDULER_NAME} \
        --location="$REGION" \
        --project="${PROJECT_ID}" \
        2>/dev/null || echo "No scheduler job found"

    # Check if scheduler is enabled or disabled
    echo -e "\nAll scheduler jobs for this function:"
    gcloud scheduler jobs list \
        --location="$REGION" \
        --project="${PROJECT_ID}" | grep ${SERVICE_NAME} || echo "No scheduler jobs found"

    # Check cloud functions status
    echo -e "\nCloud Functions status:"
    gcloud functions list \
        --regions="$REGION" \
        --project="${PROJECT_ID}" || echo "No functions found"
    
    # Check for active executions
    echo -e "\nRecent executions (last 5):"
    gcloud functions logs read ${SERVICE_NAME} \
        --region="$REGION" \
        --project="${PROJECT_ID}" \
        --limit=5 \
        --sort-by=~time
}

test() {
    echo "Testing the pipeline with a single invocation..."
    
    COLLECTION=${2:-"quotes_v7"}  # Default to quotes_v0
    
    echo "Using collection: $COLLECTION"
    
    curl -X POST "${SERVICE_URL}?collection=${COLLECTION}" \
        -H "Content-Type: application/json" \
        -d "{}" \
        --silent
    echo -e "\nTest invocation sent. Check logs for results."
}

run_now() {
    echo "Forcing immediate execution of the pipeline..."
    gcloud scheduler jobs run ${SCHEDULER_NAME} \
        --location="$REGION" \
        --project="${PROJECT_ID}"
    echo "Execution triggered. Check logs for results."
}

case "$1" in
    start)
        start "$@"
        ;;
    stop)
        stop
        ;;
    disable)
        disable
        ;;
    update)
        update "$@"
        ;;
    status)
        status
        ;;
    test)
        test "$@"
        ;;
    run_now)
        run_now
        ;;
    *)
        echo "Usage: $0 {start|stop|disable|update <minutes>|status|test [collection]|run_now}"
        exit 1
        ;;
esac