from django.urls import path
from . import views

urlpatterns = [
    path("dashboard", views.receive_data, name="recieve_data"), 
]