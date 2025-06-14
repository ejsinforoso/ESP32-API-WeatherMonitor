from flask import Flask, request, jsonify
from flask_cors import CORS
from supabase import create_client, Client
import os
from datetime import datetime

app = Flask(__name__)
CORS(app)

# Supabase configuration
SUPABASE_URL = os.getenv('SUPABASE_URL')
SUPABASE_KEY = os.getenv('SUPABASE_KEY')
supabase: Client = create_client(SUPABASE_URL, SUPABASE_KEY)

@app.route('/data', methods=['POST'])
def receive_data():
    try:
        data = request.get_json()
        
        # Insert data into Supabase
        response = supabase.table('sensor_data').insert({
            'temperature': data['temperature'],
            'humidity': data['humidity'],
            'dew_point': data['dew_point'],
            'wet_bulb': data['wet_bulb']
        }).execute()
        
        return jsonify({'message': 'Data saved successfully'}), 200
    
    except Exception as e:
        print(f"Error: {str(e)}")
        return jsonify({'error': 'Failed to save data'}), 500

@app.route('/data', methods=['GET'])
def get_data():
    try:
        # Get the last 100 records, newest first
        response = supabase.from_('sensor_data').select('*').order('created_at', desc=True).limit(100).execute()
        
        # Return the data in chronological order (oldest first)
        return jsonify({'data': list(reversed(response.data))}), 200
    
    except Exception as e:
        print(f"Error: {str(e)}")
        return jsonify({'error': 'Failed to fetch data'}), 500

if __name__ == '__main__':
    app.run(debug=True)