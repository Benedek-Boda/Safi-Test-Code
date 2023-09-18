function doGet(e){
  Logger.log("--- doGet ---");
 
 var deviceid = "",
     location = "",
     status = "",
     maxtemp = " ",
     instance = " ";
 
  try {
 
    // this helps during debuggin
    if (e == null){e={}; e.parameters = {deviceid:"test",location:"-1",status: "test2",maxtemp: "test3", instance: "test4"};}
 
    deviceid = e.parameters.deviceid;
    location = e.parameters.location;
    status = e.parameters.status;
    maxtemp = e.parameters.maxtemp;
    instance = e.parameters.instance;
 
    // save the data to spreadsheet
    save_data(deviceid, location, status, maxtemp, instance);
 
 
    return ContentService.createTextOutput("Wrote:\n  deviceid: " + deviceid + "\n  location: " + location + "\n status: " + status + "\n maxtemp: " + maxtemp + "\n instance: " + instance);
 
  } catch(error) { 
    Logger.log(error);    
    return ContentService.createTextOutput("oops...." + error.message 
                                            + "\n" + new Date() 
                                            + "\ndeviceid: " + deviceid +
                                            + "\nlocation: " + location +
                                            + "\nstatus: " + status + 
                                            + "\nmaxtemp: " + maxtemp +
                                            + "\ninstance: " + instance);
  }  
}
 
// Method to save given data to a sheet
function save_data(deviceid, location, status, maxtemp, instance){
  Logger.log("--- save_data ---"); 
 
 
  try {
    var dateTime = new Date();
 
    // Paste the URL of the Google Sheets starting from https thru /edit
    // For e.g.: https://docs.google.com/..../edit 
    var ss = SpreadsheetApp.openByUrl("https://docs.google.com/spreadsheets/d/1bbSVUUt7goUxm8G_6407Z7Us88JK155dtZs3slkLRqM/edit");
    var dataLoggerSheet = ss.getSheetByName("Safi_Datacollection_Test");
 
    // Get last edited row from DataLogger sheet
    var row = dataLoggerSheet.getLastRow() + 1;

    var lastrow = dataLoggerSheet.getLastRow()

    for (var i = 1; i <= lastrow; i++){
      var cellA = dataLoggerSheet.getRange("A" + i);
      var tempinstance = cellA.getValue();
      var cellC = dataLoggerSheet.getRange("C" + i);
      var tempdeviceid = cellC.getValue();

      if (tempinstance == instance && tempdeviceid == deviceid){
        row = i;
      }

    }
    
    // Start Populating the data
    dataLoggerSheet.getRange("A" + row).setValue(instance); // instance
    dataLoggerSheet.getRange("B" + row).setValue(dateTime); // dateTime
    dataLoggerSheet.getRange("C" + row).setValue(deviceid); // deviceid
    dataLoggerSheet.getRange("D" + row).setValue(location); // location
    dataLoggerSheet.getRange("E" + row).setValue(status); // status
    dataLoggerSheet.getRange("F" + row).setValue(maxtemp); // maxtemp
 
 
    // Update summary sheet
    summarySheet.getRange("B1").setValue(dateTime); // Last modified date
    // summarySheet.getRange("B2").setValue(row - 1); // Count 
  }
 
  catch(error) {
    Logger.log(JSON.stringify(error));
  }
 
  Logger.log("--- save_data end---"); 
}
