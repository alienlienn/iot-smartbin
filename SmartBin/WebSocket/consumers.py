from channels.generic.websocket import AsyncWebsocketConsumer
import json

class DashboardConsumer(AsyncWebsocketConsumer):
    async def connect(self):
        # Join the dashboard group
        await self.channel_layer.group_add(
            "dashboard",
            self.channel_name
        )
        await self.accept()

    async def disconnect(self, close_code):
        # Leave the dashboard group
        await self.channel_layer.group_discard(
            "dashboard",
            self.channel_name
        )

    # Receive message from the dashboard group
    async def dashboard_update(self, event):
        # Send message to WebSocket
        await self.send(text_data=json.dumps(event["data"]))