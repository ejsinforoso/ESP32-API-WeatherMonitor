from flask import Flask, request, jsonify
from flask_cors import CORS
from supabase import create_client, Client
import os
from dotenv import load_dotenv
import logging
from datetime import datetime

# Load environment variables
load_dotenv()

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)

app = Flask(__name__)
CORS(app, resources={
    r"/data": {"origins": "*"},
    r"/*": {"origins": ""}
})

# Get environment variables
SUPABASE_URL = os.environ.get('SUPABASE_URL')
SUPABASE_KEY = os.environ.get('SUPABASE_KEY')

if not SUPABASE_URL or not SUPABASE_KEY:
    logger.error("Supabase credentials not configured")
    raise ValueError("Supabase URL and Key must be set in environment variables")

# Initialize Supabase client
try:
    supabase: Client = create_client(SUPABASE_URL, SUPABASE_KEY)
    logger.info("Supabase client initialized successfully")
except Exception as e:
    logger.error(f"Failed to initialize Supabase client: {str(e)}")
    raise

@app.route('/')
def home():
    return jsonify({
        "message": "Environmental Monitoring API",
        "endpoints": {
            "POST /data": "Submit sensor data",
            "GET /data": "Retrieve sensor data"
        }
    }), 200

@app.route('/data', methods=['POST'])
def receive_data():
    try:
        # Log incoming request
        client_ip = request.remote_addr
        logger.info(f"Incoming POST from {client_ip}")
        
        data = request.get_json()
        if not data:
            logger.warning("Empty payload received")
            return jsonify({'error': 'No data provided'}), 400
        
        # Log complete payload
        logger.info(f"Received payload: {data}")
        
        # Validate required fields
        required_fields = ['temperature', 'humidity', 'dew_point', 'wet_bulb']
        if not all(field in data for field in required_fields):
            logger.warning(f"Missing fields in payload. Received: {list(data.keys())}")
            return jsonify({'error': 'Missing required fields'}), 400
        
        # Insert data into Supabase
        response = supabase.table('sensor_data').insert({
            'temperature': data['temperature'],
            'humidity': data['humidity'],
            'dew_point': data['dew_point'],
            'wet_bulb': data['wet_bulb']
        }).execute()
        
        logger.info("Data successfully saved to Supabase")
        return jsonify({
            'message': 'Data saved successfully',
            'received_data': data  # Echo back the received data for confirmation
        }), 200
    
    except Exception as e:
        logger.error(f"Error processing data: {str(e)}", exc_info=True)
        return jsonify({
            'error': 'Failed to save data',
            'details': str(e)
        }), 500

@app.route('/data', methods=['GET'])
def get_data():
    try:
        logger.info("Processing GET request for sensor data")
        
        # Get the last 100 records
        response = supabase.from_('sensor_data') \
                         .select('*') \
                         .order('created_at', desc=True) \
                         .limit(100) \
                         .execute()
        
        if not response.data:
            logger.info("No data found in database")
            return jsonify({'message': 'No data available'}), 200
        
        # Return data in chronological order
        reversed_data = list(reversed(response.data))
        logger.info(f"Returning {len(reversed_data)} records")
        return jsonify({'data': reversed_data}), 200
    
    except Exception as e:
        logger.error(f"Error fetching data: {str(e)}", exc_info=True)
        return jsonify({
            'error': 'Failed to fetch data',
            'details': str(e)
        }), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=int(os.environ.get('PORT', 5000)))
