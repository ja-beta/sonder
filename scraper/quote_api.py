import functions_framework
from display_queue import serve_quote

@functions_framework.http
def serve_quote_api(request):
    """HTTP Cloud Function that serves quotes to e-paper displays."""
    
    if request.method == 'GET':
        device_id = request.args.get('device_id', 'unknown')
    else:
        try:
            request_json = request.get_json(silent=True)
            device_id = request_json.get('device_id', 'unknown')
        except:
            device_id = 'unknown'
    
    print(f"Quote requested by device: {device_id}")
    
    quote = serve_quote(device_id)
    
    if not quote:
        return {'success': False, 'message': 'No quotes available'}
    
    return {
        'success': True,
        'quote': quote['text'],
        'source': quote['source'],
        'quote_id': quote['id']
    } 