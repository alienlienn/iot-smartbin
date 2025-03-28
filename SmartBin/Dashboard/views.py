from django.shortcuts import render
from django.http import HttpResponse, JsonResponse
from django.views.decorators.csrf import csrf_exempt
import json

# Create your views here.
def dashboard(request):
    return render(request, "dashboard.html")

def bin_detail(request, bin_id):
    return render(request, "bin_detail.html", {"bin_id": bin_id})