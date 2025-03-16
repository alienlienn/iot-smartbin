"""
ASGI config for SmartBin project.

It exposes the ASGI callable as a module-level variable named ``application``.

For more information on this file, see
https://docs.djangoproject.com/en/4.2/howto/deployment/asgi/
"""

import os
from django.core.asgi import get_asgi_application
from channels.routing import ProtocolTypeRouter, URLRouter
from channels.auth import AuthMiddlewareStack
import WebSocket.routing  # Import your WebSocket routing

os.environ.setdefault('DJANGO_SETTINGS_MODULE', 'SmartBin.settings')

application = ProtocolTypeRouter({
    "http": get_asgi_application(),  # Handle HTTP requests
    "websocket": AuthMiddlewareStack(
        URLRouter(WebSocket.routing.websocket_urlpatterns)  # Handle WebSockets
    ),
})