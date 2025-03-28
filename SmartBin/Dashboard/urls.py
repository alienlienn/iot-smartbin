from django.urls import path
from . import views

urlpatterns = [
    path("", views.dashboard, name="dashboard"), 
    path("bin/<int:bin_id>", views.bin_detail, name="bin_detail"),
]