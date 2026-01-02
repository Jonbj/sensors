module.exports = {
  // Enable admin auth (Node-RED editor protection).
  // Generate the bcrypt hash with:
  //   docker exec -it nodered node-red-admin hash-pw
  adminAuth: {
    type: "credentials",
    users: [{
      username: "admin",
      password: "$2b$08$REPLACE_WITH_REAL_BCRYPT_HASH",
      permissions: "*"
    }]
  }
}
