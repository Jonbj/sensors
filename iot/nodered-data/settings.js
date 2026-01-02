module.exports = {
  // Enable admin auth (Node-RED editor protection).
  // Generate the bcrypt hash with:
  //   docker exec -it nodered node-red-admin hash-pw
  adminAuth: {
    type: "credentials",
    users: [{
      username: "admin",
      password: "2y$08$IYIy6.HcW7Yj28aTkW34fOh/z4cspRye28VyBfflRjWquLw6BAYky",
      permissions: "*"
    }]
  }
}
