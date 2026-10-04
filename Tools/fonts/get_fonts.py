"""Fetch the UI fonts (SIL Open Font License 1.1, Google Fonts repository) into Art/Fonts and cut static faces.

  python Tools/fonts/get_fonts.py

IM Fell English (Regular, Italic) and IM Fell English SC come as static TTFs. Alegreya comes as variable fonts; Unreal
imports a variable font's default instance only, so this cuts static Regular (400), Bold (700) and Italic (400) faces
with fontTools. The licences (OFL.txt) are kept next to the fonts; see Docs/ThirdParty.md.
"""
import os
import urllib.request

from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Art", "Fonts")
BASE = "https://raw.githubusercontent.com/google/fonts/main/ofl"
FILES = {
    "IMFellEnglish": [("imfellenglish", "IMFeENrm28P.ttf"), ("imfellenglish", "IMFeENit28P.ttf"), ("imfellenglish", "OFL.txt")],
    "IMFellEnglishSC": [("imfellenglishsc", "IMFeENsc28P.ttf"), ("imfellenglishsc", "OFL.txt")],
    "Alegreya": [("alegreya", "Alegreya[wght].ttf"), ("alegreya", "Alegreya-Italic[wght].ttf"), ("alegreya", "OFL.txt")],
}


def fetch():
    for family, files in FILES.items():
        os.makedirs(os.path.join(OUT, family), exist_ok=True)
        for folder, name in files:
            path = os.path.join(OUT, family, name)
            if os.path.exists(path):
                continue
            url = "%s/%s/%s" % (BASE, folder, urllib.request.quote(name))
            print("download", url)
            urllib.request.urlretrieve(url, path)


def cut_alegreya():
    src = os.path.join(OUT, "Alegreya")
    for var, name, weight in (("Alegreya[wght].ttf", "Alegreya-Regular.ttf", 400), ("Alegreya[wght].ttf", "Alegreya-Bold.ttf", 700),
                              ("Alegreya-Italic[wght].ttf", "Alegreya-Italic.ttf", 400)):
        font = TTFont(os.path.join(src, var))
        static = instancer.instantiateVariableFont(font, {"wght": weight}, updateFontNames=True)
        static.save(os.path.join(src, name))
        print("cut", name)


if __name__ == "__main__":
    fetch()
    cut_alegreya()
