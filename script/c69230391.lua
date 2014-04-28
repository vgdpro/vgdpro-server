--爆炎帝テスタロス
function c69230391.initial_effect(c)
	--summon with 1 tribute
	local e1=Effect.CreateEffect(c)
	e1:SetDescription(aux.Stringid(69230391,0))
	e1:SetProperty(EFFECT_FLAG_CANNOT_DISABLE+EFFECT_FLAG_UNCOPYABLE)
	e1:SetType(EFFECT_TYPE_SINGLE)
	e1:SetCode(EFFECT_SUMMON_PROC)
	e1:SetCondition(c69230391.otcon)
	e1:SetOperation(c69230391.otop)
	e1:SetValue(SUMMON_TYPE_ADVANCE)
	c:RegisterEffect(e1)
	local e2=e1:Clone()
	e2:SetCode(EFFECT_SET_PROC)
	c:RegisterEffect(e2)
	--handes
	local e3=Effect.CreateEffect(c)
	e3:SetDescription(aux.Stringid(69230391,1))
	e3:SetCategory(CATEGORY_HANDES+CATEGORY_DAMAGE)
	e3:SetType(EFFECT_TYPE_SINGLE+EFFECT_TYPE_TRIGGER_F)
	e3:SetCode(EVENT_SUMMON_SUCCESS)
	e3:SetCondition(c69230391.condition)
	e3:SetTarget(c69230391.target)
	e3:SetOperation(c69230391.operation)
	c:RegisterEffect(e3)
end
function c69230391.otfilter(c)
	return bit.band(c:GetSummonType(),SUMMON_TYPE_ADVANCE)==SUMMON_TYPE_ADVANCE
end
function c69230391.otcon(e,c)
	if c==nil then return true end
	local g=Duel.GetTributeGroup(c)
	return c:GetLevel()>6 and Duel.GetLocationCount(c:GetControler(),LOCATION_MZONE)>-1
		and g:IsExists(c69230391.otfilter,1,nil)
end
function c69230391.otop(e,tp,eg,ep,ev,re,r,rp,c)
	local g=Duel.GetTributeGroup(c)
	Duel.Hint(HINT_SELECTMSG,tp,HINTMSG_RELEASE)
	local sg=g:FilterSelect(tp,c69230391.otfilter,1,1,nil)
	c:SetMaterial(sg)
	Duel.Release(sg,REASON_SUMMON+REASON_MATERIAL)
end
function c69230391.condition(e,tp,eg,ep,ev,re,r,rp)
	return e:GetHandler():GetSummonType()==SUMMON_TYPE_ADVANCE
end
function c69230391.target(e,tp,eg,ep,ev,re,r,rp,chk)
	if chk==0 then return true end
	Duel.SetOperationInfo(0,CATEGORY_HANDES,nil,0,1-tp,1)
	local mg=e:GetHandler():GetMaterial()
	if mg:IsExists(Card.IsAttribute,1,nil,ATTRIBUTE_FIRE) then
		e:SetLabel(1)
		Duel.SetOperationInfo(0,CATEGORY_DAMAGE,nil,0,1-tp,1000)
	else
		e:SetLabel(0)
	end
end
function c69230391.operation(e,tp,eg,ep,ev,re,r,rp)
	local g=Duel.GetFieldGroup(tp,0,LOCATION_HAND)
	if g:GetCount()==0 then return end
	Duel.ConfirmCards(tp,g)
	Duel.Hint(HINT_SELECTMSG,tp,HINTMSG_DISCARD)
	local hg=g:Select(tp,1,1,nil)
	Duel.SendtoGrave(hg,REASON_EFFECT+REASON_DISCARD)
	Duel.ShuffleHand(1-tp)
	local tc=hg:GetFirst()
	if tc:IsType(TYPE_MONSTER) then
		Duel.Damage(1-tp,tc:GetLevel()*200,REASON_EFFECT)
	end
	if e:GetLabel()==1 then
		Duel.Damage(1-tp,1000,REASON_EFFECT)
	end
end