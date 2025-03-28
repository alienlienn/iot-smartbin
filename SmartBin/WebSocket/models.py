from django.db import models
import json

class SensorData(models.Model):
    device_id = models.CharField(max_length=20)
    timestamp = models.DateTimeField(auto_now_add=True)
    last_seen = models.IntegerField(null=True)
    status = models.CharField(max_length=20, null=True)
    
    # Store coordinates as a JSON string
    coord_json = models.TextField(null=True, blank=True)
    next_nearest = models.CharField(max_length=20, null=True, blank=True)
    next_nearest_direction = models.CharField(max_length=2, null=True, blank=True)

    class Meta:
        ordering = ['-timestamp']
    
    # Property methods to handle the JSON serialization/deserialization
    @property
    def coord(self):
        if self.coord_json:
            return json.loads(self.coord_json)
        return None
    
    @coord.setter
    def coord(self, value):
        if value is not None:
            self.coord_json = json.dumps(value)
        else:
            self.coord_json = None