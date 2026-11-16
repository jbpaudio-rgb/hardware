# Builds fx.html from the VACT page shell plus this rack's params / DSP / pages / tail (the shell is only used by the Octavio build)
v=open(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.dirname(__import__('os').path.abspath(__file__))),'vact-web','vact.html')).read()
P=open('params.js').read();W=open('dsp.js').read();U=open('ui.js').read();T=open('tail.js').read()
a=v.index('// ===================== Parameter table');b=v.index('// ===================== Audio setup')
v=v[:a]+P+W+'\n'+v[b:]
a=v.index('// ===================== Pages');b=v.index("const M=document.getElementById('mod');")+len("const M=document.getElementById('mod');")
v=v[:a]+U+v[b:]
a=v.index('pickSeq(0);')+len('pickSeq(0);');b=v.index('// ===================== Keyboard')
v=v[:a]+T+v[b:]
v=v.replace('<title>Octavio VACT</title>','<title>Octavio FX</title>')
open('fx.html','w').write(v)
print('fx.html',len(v))
