def is_isogram(string):
    lString = string.lower()
    used_chars = []
    if len(lString) == 0:
       return True
    for char in lString:
       if char in used_chars:
          return False
       else:
          used_chars.append(char)
    return True

print( is_isogram("Dermatoglyphics") )
