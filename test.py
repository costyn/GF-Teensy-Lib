class Person():

    def __init__(self, first_name, last_name, age):
        self.first_name = first_name
        self.last_name = last_name
        self.age = age
        
    def full_name():
        return self.first_name + " " + self.last_name

matz = Person('Yukihiro', 'Matsumoto', 47)
print( matz.full_name)

joe = Person('Joe', 'Smith', 30)
print(joe.full_name)
