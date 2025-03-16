from django.db import models

class SensorData(models.Model):
    device_id = models.IntegerField(default=0)
    last_seen = models.IntegerField()
    status = models.CharField(max_length=50)
    coord = models.JSONField(null=True, blank=True)

    def __str__(self):
        return f"Device {self.device_id} - {self.status}"