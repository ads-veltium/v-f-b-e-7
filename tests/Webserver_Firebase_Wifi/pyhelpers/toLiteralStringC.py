theString = """{
  "kind": "identitytoolkit#VerifyPasswordResponse",
  "localId": "WW1zp2ptgIOK2zxFUWUzjcDTFFq1",
  "email": "grantasca@yahoo.es",
  "displayName": "",
  "idToken": "eyJhbGciOiJSUzI1NiIsImtpZCI6IjgyMmM1NDk4YTcwYjc0MjQ5NzI2ZDhmYjYxODlkZWI3NGMzNWM4MGEiLCJ0eXAiOiJKV1QifQ.eyJpc3MiOiJodHRwczovL3NlY3VyZXRva2VuLmdvb2dsZS5jb20vdmVsdGl1bWJhY2tlbmQiLCJhdWQiOiJ2ZWx0aXVtYmFja2VuZCIsImF1dGhfdGltZSI6MTU5MDY2MTI2MSwidXNlcl9pZCI6IldXMXpwMnB0Z0lPSzJ6eEZVV1V6amNEVEZGcTEiLCJzdWIiOiJXVzF6cDJwdGdJT0syenhGVVdVempjRFRGRnExIiwiaWF0IjoxNTkwNjYxMjYxLCJleHAiOjE1OTA2NjQ4NjEsImVtYWlsIjoiZ3JhbnRhc2NhQHlhaG9vLmVzIiwiZW1haWxfdmVyaWZpZWQiOnRydWUsImZpcmViYXNlIjp7ImlkZW50aXRpZXMiOnsiZW1haWwiOlsiZ3JhbnRhc2NhQHlhaG9vLmVzIl19LCJzaWduX2luX3Byb3ZpZGVyIjoicGFzc3dvcmQifX0.BobAey062GP3urGNEtiLaE6toXbDjxochZrPyfY9j-yJFixZgM7gQKKQXu6J3r40psbS1v8BzogFMY51ctenQ67Ui7WY3q66vUEsBzYudQ7Lq9iUeUW_O4gFN_Ex4IUN6pq0hR7iqaqO-4Y6BJKYaJS2OOZkv0j46RV3EZAadmsMBVQAsz09NZbWj8cw1ZQii-tHDdV1nvaWWffl4kuP5LhfTD7B-gaNyK6V02R04bQWmFxKX5XPS0n6OJE_NTALt97ILXM3szBpWRtkyYPO77x770hi4IFANbdZnAdLhaVPnFGy6mI6URDzAoMAOK903lro7-H3pJxCQKw5eRgoxQ",
  "registered": true,
  "refreshToken": "AE0u-Nf3xoqhmLd6ZaZWw5TZduWbxmO_IDYoEZWSUN9CP1_SZic-W0wnnyyIhdBGivq5kjuc2zOXRzdnFyjCb2obKJnDZwxRBgdgBH1nJRXgBQObYzN994NGD3n5vQ0RFJ9-_wwzlq964X8Y9QxAHMgdK-2Q1i7dX4DKXN7rWO6nA-mcFraRSYnrnR1I9UJxbfVBhTduxGtd373PPc--0mPBsvD0K0pjEw",
  "expiresIn": "3600"
}
"""

theVarName = 'exampleJSON'

result = 'const char* ' + theVarName + ' = ""\\\n'
for line in theString.splitlines():
    line = line.replace('"', '\\"')
    result += '"' + line + '\\n"\\\n'

print(result)
