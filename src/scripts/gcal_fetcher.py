import datetime
import os.path
import json

from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build

# If modifying these scopes, delete the file token.json.
SCOPES = ['https://www.googleapis.com/auth/calendar.readonly']

CONFIG_DIR = os.path.expanduser('~/.config/my_wallpaper_engine')
CACHE_DIR = os.path.expanduser('~/.cache/my_wallpaper_engine')
CREDENTIALS_FILE = os.path.join(CONFIG_DIR, 'credentials.json')
TOKEN_FILE = os.path.join(CONFIG_DIR, 'token.json')
OUTPUT_FILE = os.path.join(CACHE_DIR, 'events.txt')

def main():
    if not os.path.exists(CACHE_DIR):
        os.makedirs(CACHE_DIR)

    creds = None
    # The file token.json stores the user's access and refresh tokens, and is
    # created automatically when the authorization flow completes for the first
    # time.
    if os.path.exists(TOKEN_FILE):
        creds = Credentials.from_authorized_user_file(TOKEN_FILE, SCOPES)
    # If there are no (valid) credentials available, let the user log in.
    if not creds or not creds.valid:
        if creds and creds.expired and creds.refresh_token:
            creds.refresh(Request())
        else:
            if not os.path.exists(CREDENTIALS_FILE):
                print(f"Error: {CREDENTIALS_FILE} not found.")
                return
            flow = InstalledAppFlow.from_client_secrets_file(
                CREDENTIALS_FILE, SCOPES)
            creds = flow.run_local_server(port=0)
        # Save the credentials for the next run
        with open(TOKEN_FILE, 'w') as token:
            token.write(creds.to_json())

    try:
        service = build('calendar', 'v3', credentials=creds)

        # Call the Calendar API
        now = datetime.datetime.utcnow()
        # Fetch events from the start of the current month to the end
        start_of_month = now.replace(day=1, hour=0, minute=0, second=0, microsecond=0).isoformat() + 'Z'
        next_month = now.replace(day=28) + datetime.timedelta(days=4)
        end_of_month = next_month.replace(day=1, hour=0, minute=0, second=0, microsecond=0).isoformat() + 'Z'

        cal_ids = [
            'primary',
            'en.vietnamese#holiday@group.v.calendar.google.com',
            'en.japanese#holiday@group.v.calendar.google.com'
        ]

        all_events = []
        for cal_id in cal_ids:
            try:
                events_result = service.events().list(calendarId=cal_id, timeMin=start_of_month,
                                                      timeMax=end_of_month, maxResults=50, singleEvents=True,
                                                      orderBy='startTime').execute()
                events = events_result.get('items', [])
                
                # Prepend flag emoji based on calendar ID
                prefix = ""
                if "vietnam" in cal_id:
                    prefix = "🇻🇳 "
                elif "japan" in cal_id:
                    prefix = "🇯🇵 "
                
                for event in events:
                    event['summary'] = prefix + event.get('summary', 'Holiday')
                
                all_events.extend(events)
            except Exception as e:
                print(f"Warning: Error fetching calendar {cal_id}: {e}")

        with open(OUTPUT_FILE, 'w') as f:
            for event in all_events:
                start = event['start'].get('dateTime', event['start'].get('date'))
                # Parse to get day of month
                if 'T' in start:
                    dt = datetime.datetime.fromisoformat(start.replace('Z', '+00:00'))
                    day = dt.day
                else:
                    # All day event, format is YYYY-MM-DD
                    day = int(start.split('-')[2])
                
                title = event.get('summary', 'Busy')
                # Replace newlines or pipe chars so parsing is safe
                title = title.replace('|', '-').replace('\n', ' ')
                f.write(f"{day}|{title}\n")

        print(f"Successfully synced {len(all_events)} events to {OUTPUT_FILE}")

    except Exception as error:
        print(f"An error occurred: {error}")

if __name__ == '__main__':
    main()
