
class Export
  include MRuby::LoadGems

  attr_reader :gems

  def initialize(&block)
    @gems =  MRuby::Gem::List.new
    self.instance_eval(&block)
  end

  def root
    MRUBY_ROOT
  end
end
