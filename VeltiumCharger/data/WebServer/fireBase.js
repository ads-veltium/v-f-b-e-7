var fireBase = fireBase || firebase;
var hasInit = false;
var config = {
  apiKey: "AIzaSyCYZpVNUOQvrXvc3qETxCqX4DPfp3Fwe3w",
  authDomain: "veltiumdev.firebaseapp.com",
  projectId: "veltiumdev",
  databaseURL: "https://veltiumdev-default-rtdb.firebaseio.com",
  storageBucket: "veltiumdev.appspot.com",
  messagingSenderId: "1020185418136",
  appId: "1:1020185418136:web:6453b887d5d71329c633ed",
  measurementId: "G-VN3G1YL6TX"
  };
if(!hasInit){
    firebase.initializeApp(config);
    hasInit = true;
}


