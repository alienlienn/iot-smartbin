from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
import json
from .models import SensorData
from channels.layers import get_channel_layer
from asgiref.sync import async_to_sync

@csrf_exempt
def receive_data(request):
    if request.method == 'POST':
        try:
            data = json.loads(request.body)
            
            records = []
            for device_id, values in data.items():
                records.append(SensorData(
                    device_id=device_id,
                    last_seen=values.get("last_seen"),
                    status=values.get("status"),
                    coord=values.get("coord")
                ))

            SensorData.objects.bulk_create(records)

            channel_layer = get_channel_layer()
            async_to_sync(channel_layer.group_send)(
                "dashboard",
                {"type": "dashboard_update", "data": data}
            )

            return JsonResponse({"message": "Data received"}, status=201)
        except json.JSONDecodeError:
            return JsonResponse({"error": "Invalid JSON format"}, status=400)
        except Exception as e:
            return JsonResponse({"error": str(e)}, status=500)

    return JsonResponse({"error": "Only POST requests allowed"}, status=405)