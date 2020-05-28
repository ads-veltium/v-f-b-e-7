#filenames = ['login', 'root']
filenames = ['root', 'login', 'auth_fail', 'auth_ok', 'user_root']

result = ''
endl = '\n';

for fn in filenames:
    f = open(fn+'.html', 'r')
    txt = f.read();
    f.close();

    result += 'static const char html_' + fn + '[] = \\' + endl;
    for line in txt.splitlines():
        line = line.replace('"', '\\"');
        result += '"' + line + '\\n"\\\n';
    result += ';' + endl;


#theVarName = 'exampleJSON'

#result = 'const char* ' + theVarName + ' = ""\\\n'
#for line in theString.splitlines():
#    line = line.replace('"', '\\"')
#    result += '"' + line + '\\n"\\\n'

print(result)






file_comment = """////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  VeltiumAdminWebserver.ext
//  Veltium Smart Charger on ESP32
//
//  Created by David Crespo on 28/05/2020.
//  Copyright © 2020 Virtual Code SL. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
"""

header_file_template = """
class VeltiumHTMLStrings
{
PLACEHOLDER_DECLS;
};
"""

